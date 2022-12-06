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
from sklearn.ensemble import RandomForestClassifier

pd.set_option('mode.chained_assignment',  None) # 경고 off

parser = argparse.ArgumentParser()
parser.add_argument('input', type=str, nargs="+")
parser.add_argument('--permote', action="store_true")

conopts = parser.parse_args()
import sys
if len(conopts.input) == 1 and isinstance(conopts.input, list):
    conopts.input = conopts.input[0]
if sys.platform.startswith("win") and "*" in conopts.input:
    conopts.input = glob.glob(conopts.input)

# Read data
if not isinstance(conopts.input, list) and conopts.input.endswith(".csv"):
    assert (os.path.exists(conopts.input))
    df_data = pd.read_csv(conopts.input)
    print("Read preprocessed data fin")
else:
    if not isinstance(conopts.input, list):
        conopts.input = [conopts.input]
    conopts.input = [i for i in conopts.input if os.path.isdir(i)]
    simul_dirs = conopts.input

    window_size = 10 # feature input chunk size
    detect_period = 5 # period to run detection
    attack_standard_idx = 2 # attack이라고 판단할 frame 번호. 숫자 클수록 빠르게 반응

    feature_cols = ['Time', 'Mote', 
                        'Seq', 'Rank', 'Version', 
                        'DIS-UR', 'DIS-MR', 'DIS-US', 'DIS-MS', 
                        'DIO-UR', 'DIO-MR', 'DIO-US', 'DIO-MS', 
                        'DAO-R', 'DAO-S', 'DAOA-R', 'DAOA-S', 'dio_intcurrent','dio_counter']
    #['Time', 'Mote', 'Seq', 'Rank', 'Version', 'DIS-R', 'DIS-S', 'DIO-R', 'DIO-S', 'DAO-R', 'RPL-total-sent']
    meta_cols = ['Attack', 'Trxr']


    # Feature engineering and data chunking
    all_data, all_metadata = [], []
    for si, simul_dir in enumerate(simul_dirs):
        print(si+1, len(simul_dirs))
        df = pd.read_csv(f'{simul_dir}/rpl-statistics.csv')
        
        # Get configs from simul name
        m = re.search(r'trxr.*?(-|$)', simul_dir)
        cfg_trxr = m.group().split("_")[1][:-1]
        
        # Permote processing
        if conopts.permote:
            permote_col = ['Time',
                        'DIS-UR', 'DIS-MR', 'DIS-US', 'DIS-MS', 
                        'DIO-UR', 'DIO-MR', 'DIO-US', 'DIO-MS', 
                        'DAO-R', 'DAO-S', 'DAOA-R', 'DAOA-S', 'dio_intcurrent','dio_counter']
            # ['Time', 'DIS-R', 'DIS-S', 'DIO-R', 'DIO-S', 'DAO-R', 'RPL-total-sent']
            for moteIdx in np.unique(df['Mote']):
                _df_mote = df[df['Mote'] == moteIdx]
                df_mote = _df_mote.copy()
                for rowIdx in range(len(df_mote.index)):
                    if rowIdx != 0:
                        for col in permote_col:
                            df_mote[col].iloc[rowIdx] = _df_mote[col].iloc[rowIdx] - _df_mote[col].iloc[rowIdx -1]
                
                df[df['Mote'] == moteIdx] = df_mote
            
        df_split = [None] * len(df.index)
        datas = []
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
                
                # if d == 0:
                #     time_base = df_el['Time0'].iloc[0]
                # df_el['Time'+str(d)] -= time_base
                
            
            df_metadata = pd.DataFrame([[df_split[row_idx + attack_standard_idx]['Attack'].item(), cfg_trxr]], columns=meta_cols)
            df_perrow = pd.concat(df_perrow + [df_metadata], axis=1)
            datas.append(df_perrow)
        
        all_data.append(pd.concat(datas))

    df_data = pd.concat(all_data)
    csv_path = f"{simul_dir}/../data-ws{window_size}-dp{detect_period}-asi{attack_standard_idx}{'-pm' if conopts.permote else ''}.csv"
    df_data.to_csv(csv_path)
    print("Data process fin")
    df_data = pd.read_csv(csv_path)

    conopts.input = csv_path

# print(df_data)
# -- filter cols
feature_cols = [
    #'Time',
                        'DIS-UR', 'DIS-MR', 'DIS-US', 'DIS-MS', 
                        'DIO-UR', 'DIO-MR', 'DIO-US', 'DIO-MS', 
                        #'DAO-R', 'DAO-S', 'DAOA-R', 'DAOA-S', 
                        'dio_intcurrent',
                        #'dio_counter',
                        ]

# [
#     # 'Time', 
#     'Mote', 
#     # 'Seq', 'Rank',
#     'DIS-R', 'DIS-S', 'DIO-R', 'DIO-S', 'DAO-R',
#     # 'RPL-total-sent'
#     ]
cols = []
for c in df_data.columns:
    for fc in feature_cols:
        if fc in c:
            cols.append(c)

# -- filter rows
trxrs = [0.7, 0.8, 0.9, 1.0]
df_data = df_data[np.isin(df_data['Trxr'], trxrs)]
# df_data = df_data[np.isin(df_data['Attack'], ['No', "dio-drop"])]

# -- get data
X = df_data[cols]
Y = df_data['Attack']
# -- PCA
pca_n_comp =   2 # pca dimension (2 or 3)
pca = PCA(n_components=pca_n_comp, whiten=True).fit(X)
X_pca = pca.transform(X)
print('explained variance ratio:',pca.explained_variance_ratio_)
print('Preserved Variance:',sum(pca.explained_variance_ratio_))
pcacol = [[f'pca {i+1}' for i in range(pca_n_comp)]]
principalDf = pd.DataFrame(data = X_pca, columns = pcacol)

colors = cycle('rgb')
target_names = np.unique(Y)
if True:
    fg = pl.figure(figsize=(16, 9))
    for fi, trxr in enumerate(trxrs):
        print(fi, trxr)
        ax = fg.add_subplot(2, 2, fi + 1)

        tix = df_data['Trxr'] == trxr
        target_list = np.array(Y[tix]).flatten()
        _X_pca = X_pca[tix]
        for t_name, c in zip(target_names, colors):
                ax.scatter(_X_pca[target_list == t_name, 0], _X_pca[target_list == t_name,1],
                c=c, label=t_name, s=2)
        ax.legend(target_names)
        ax.set_xlabel('pca 1')
        ax.set_ylabel('pca 2')
        ax.set_title(trxr)
        
else:
    fg = pl.figure()
    ax = fg.add_subplot(projection='3d') if pca_n_comp == 3 else fg.add_subplot()

    target_list = np.array(Y).flatten()
    for t_name, c in zip(target_names, colors):
            if pca_n_comp == 2:
                ax.scatter(X_pca[target_list == t_name, 0], X_pca[target_list == t_name,1],
                c=c, label=t_name, s=2)
            elif pca_n_comp == 3:
                ax.scatter(X_pca[target_list == t_name, 0], X_pca[target_list == t_name,1], 
                        X_pca[target_list == t_name, 2], c=c, label=t_name)
    ax.legend(target_names)
    ax.set_xlabel('pca 1')
    ax.set_ylabel('pca 2')
    if pca_n_comp == 3:
        ax.set_zlabel('pca 3')

# pl.show()
pl.suptitle(conopts.input.replace(".csv", ""))
pl.tight_layout()
pl.savefig(conopts.input.replace(".csv", ".png"))
# -- run model

clf = svm.SVC(kernel='rbf', C=1, verbose=True, shrinking=0)
scores_res = model_selection.cross_val_score(clf, X, Y,cv=5)

print(scores_res)
print(scores_res.mean())

model = RandomForestClassifier(n_estimators=100, random_state=0)
scores_res = model_selection.cross_val_score(model, X, Y,cv=5)

print(scores_res)
print(scores_res.mean())
