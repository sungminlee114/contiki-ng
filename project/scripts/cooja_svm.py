import argparse
import glob
import os
import re
import pandas as pd
from sklearn import model_selection
from sklearn import svm
from sklearn.decomposition import PCA
import matplotlib.pyplot as pl
from itertools import cycle
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument('input', type=str)

conopts = parser.parse_args()
import sys
if sys.platform.startswith("win") and "*" in conopts.input:
    conopts.input = glob.glob(conopts.input)

if not isinstance(conopts.input, list):
    conopts.input = [conopts.input]
conopts.input = [i for i in conopts.input if os.path.isdir(i)]
simul_dirs = conopts.input

print(simul_dirs)
window_size = 3 # feature input chunk size
detect_period = 1 # period to run detection
attack_standard_idx = 2 # attack이라고 판단할 frame 번호. 숫자 클수록 빠르게 반응

feature_cols = ['Time', 'Mote', 'Seq', 'Rank', 'Version', 'DIS-R', 'DIS-S', 'DIO-R', 'DIO-S', 'DAO-R', 'RPL-total-sent']
meta_cols = ['Attack', 'Trxr']


# Feature engineering and data chunking
processed_df = pd.DataFrame()
for simul_dir in simul_dirs:
    df = pd.read_csv(f'{simul_dir}/rpl-statistics.csv',sep=';')
    
    # Get configs from simul name
    m = re.search(r'trxr.*?(-|$)', simul_dir)
    cfg_trxr = m.group().split("_")[1][:-1]
    
    df_split = [None] * len(df.index)
    datas,metadatas = [], []
    for row_idx in range(0, len(df.index)-window_size, detect_period):
        df_perrow = []
        time_base = 0
        for d in range(window_size):
            i = row_idx + d
            if df_split[i] is None:
                df_split[i] = df.iloc[i:i+1, :]
            
            df_el = df_split[i][feature_cols]
            
            df_el.columns = [idx+str(d) for idx in df_el.columns]
            df_el = df_el.reset_index(drop=True)
            df_perrow.append(df_el)
            
            if d == 0:
                time_base = df_el['Time0'].iloc[0]
            df_el['Time'+str(d)] -= time_base
            
        
        df_metadata = pd.DataFrame([[df_split[row_idx + attack_standard_idx]['Attack'].item(), cfg_trxr]], columns=meta_cols)
        df_perrow = pd.concat(df_perrow, axis=1)
        datas.append(df_perrow)
        metadatas.append(df_metadata)
    
    df_data = pd.concat(datas)
    df_metadata = pd.concat(metadatas)
    print(df_data, df_metadata)
    
    


df_total = None
print(df_total)

cooja_feature = ['DIS-R','DIS-S','DIO-R','DIO-S','DAO-R','RPL-total-sent']

X = df_total.loc[:,cooja_feature].values
Y = df_total.loc[:,['Attack']].values

X_train,X_test,Y_train,Y_test = model_selection.train_test_split(X,Y, 
                                        test_size=0.4, random_state =0)

clf_ob = svm.SVC(kernel='linear',C=1).fit(X_train, Y_train)

print(clf_ob.score(X_test, Y_test))

scores_res = model_selection.cross_val_score(clf_ob,X,Y,cv=5)

print(scores_res)

print(scores_res.mean())

# in_data_for_prediction = [[4.9876, 3.348, 1.8488, 0.2], 
#                           [5.3654, 2.0853, 3.4675, 1.1222], 
#                           [5.890, 3.33, 5.134, 1.6]]

# p_res = clf_ob.predict(in_data_for_prediction)
# print('Given first iris is of type:', p_res[0])
# print('Given second iris is of type:', p_res[1])
# print('Given third iris is of type:', p_res[2])

pca = PCA(n_components=2, whiten=True).fit(X)
X_pca = pca.transform(X)
print('explained variance ratio:',pca.explained_variance_ratio_)
print('Preserved Variance:',sum(pca.explained_variance_ratio_))

principalDf = pd.DataFrame(data = X_pca, columns = ['principal component 1', 'principal component 2'])

colors = cycle('rgb')
target_names = ['No', 'dis-repeat', 'dio-drop']

pl.figure()

target_list = np.array(Y).flatten()
for t_name, c in zip(target_names, colors):
        pl.scatter(X_pca[target_list == t_name, 0], X_pca[target_list == t_name,1]
        , c=c, label=t_name)
pl.legend(target_names)
pl.show()

