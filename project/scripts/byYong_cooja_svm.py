import pandas as pd
from sklearn import model_selection
from sklearn import svm
from sklearn.decomposition import PCA
import matplotlib.pyplot as pl
from itertools import cycle
import numpy as np
# from io import StringIO #

# rpl-udp-10-attack-flooding-dio-drop-trxr1.0-dt-1663181965745
# rpl-udp-10-attack-flooding-dio-drop-trxr0.7-dt-1663188414214
# rpl-udp-10-attack-flooding-dis-repeat-trxr1.0-dt-1663182106848 
# rpl-udp-10-attack-flooding-dis-repeat-trxr0.7-dt-1663182008894
# rpl-udp-10-attack-none-trxr1.0-dt-1663182310484
# rpl-udp-10-attack-none-trxr0.7-dt-1663182157756

sim_num = 0
simul_dir = ['udp-10-attack_diodrop-base-dt-1669733328831']


df = pd.read_csv(f'~/Documents/git/itea-2022/new_contiki/contiki-ng/project/rpl-udp-ids-flooding/ext/{simul_dir[sim_num]}/rpl-statistics.csv',
                sep=';')

# df['g'] = df.index // 3
    
# df_g= df.groupby('g').reset_index()

chunk_cnt = 10

cnt = df.index.stop // chunk_cnt
print(cnt)
column_names = ['Time', 'Mote', 
                        'Seq', 'Rank', 'Version', 
                        'DIS-UR', 'DIS-MR', 'DIS-US', 'DIS-MS', 
                        'DIO-UR', 'DIO-MR', 'DIO-US', 'DIO-MS', 
                        'DAO-R', 'DAO-S', 'DAOA-R', 'DAOA-S', 'dio_intcurrent','dio_counter' 
                        'Attack']
# ['Time', 'Mote', 'Seq', 'Rank', 'Version', 'DIS-R', 'DIS-S', 'DIO-R', 'DIO-S',
                        # 'DAO-R', 'RPL-total-sent' , 'Attack' ]
column_names_t = ['Time', 'Mote', 
                        'Seq', 'Rank', 'Version', 
                        'DIS-UR', 'DIS-MR', 'DIS-US', 'DIS-MS', 
                        'DIO-UR', 'DIO-MR', 'DIO-US', 'DIO-MS', 
                        'DAO-R', 'DAO-S', 'DAOA-R', 'DAOA-S', 'dio_intcurrent','dio_counter' ]
# ['Time', 'Mote', 'Seq', 'Rank', 'Version', 'DIS-R', 'DIS-S', 'DIO-R', 'DIO-S',
#                         'DAO-R', 'RPL-total-sent' ]
df_mod0 = pd.DataFrame(columns=column_names)
df_mod1 = pd.DataFrame(columns=column_names_t)
df_mod2 = pd.DataFrame(columns=column_names_t)

df_mod3 = pd.DataFrame(columns=column_names_t)
df_mod4 = pd.DataFrame(columns=column_names_t)

df_mod5 = pd.DataFrame(columns=column_names_t)
df_mod6 = pd.DataFrame(columns=column_names_t)

df_mod7 = pd.DataFrame(columns=column_names_t)
df_mod8 = pd.DataFrame(columns=column_names_t)
df_mod9 = pd.DataFrame(columns=column_names_t)

for N in range(cnt):
        df_mod0.loc[N] = df.iloc[N*chunk_cnt + 0,:]
        df_mod1.loc[N] = df.iloc[N*chunk_cnt + 1,:]
        df_mod2.loc[N] = df.iloc[N*chunk_cnt + 2,:]
        df_mod3.loc[N] = df.iloc[N*chunk_cnt + 3,:]
        df_mod4.loc[N] = df.iloc[N*chunk_cnt + 4,:]
        df_mod5.loc[N] = df.iloc[N*chunk_cnt + 5,:]
        df_mod6.loc[N] = df.iloc[N*chunk_cnt + 6,:]

        df_mod7.loc[N] = df.iloc[N*chunk_cnt + 7,:]
        df_mod8.loc[N] = df.iloc[N*chunk_cnt + 8,:]
        df_mod9.loc[N] = df.iloc[N*chunk_cnt + 9,:]

df_total = pd.concat([df_mod0,df_mod1,df_mod2,df_mod3,df_mod4,df_mod5,df_mod6,df_mod7,df_mod8,df_mod9], axis = 1)

print(df_total)

cooja_feature = [
    # 'Time', 'Mote', 
    #                     'Seq', 'Rank', 'Version', 
                        'DIS-UR', 'DIS-MR', 'DIS-US', 'DIS-MS', 
                        'DIO-UR', 'DIO-MR', 
                        #'DIO-US', 'DIO-MS', 
                        # 'DAO-R', 'DAO-S', 'DAOA-R', 'DAOA-S', 'dio_intcurrent','dio_counter' 
                        ]
# ['DIS-R','DIS-S','DIO-R','DIO-S','DAO-R','RPL-total-sent']

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

principalDf = pd.DataFrame(data = X_pca
             , columns = ['principal component 1', 'principal component 2'])
             
colors = cycle('rgb')
target_names = ['No', 'dis-repeat', 'dio-drop']

print('X_pca:',X_pca)

pl.figure()
pl.title(simul_dir[sim_num])
target_list = np.array(Y).flatten()
for t_name, c in zip(target_names, colors):
        pl.scatter(X_pca[target_list == t_name, 0], X_pca[target_list == t_name, 1]
        , c=c, label=t_name)
pl.legend(target_names)
pl.show()

