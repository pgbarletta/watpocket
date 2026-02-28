#!/bin/bash


# /home/pb777/26/watpocket/build/src/watpocket/watpocket complex_wcn.parm7 0.nc --resnums 164,128,160,55,167,61,42,65,66 -o 0.csv -d 0traj.pdb
# /home/pb777/26/watpocket/build/src/watpocket/watpocket complex_wcn.parm7 1.nc --resnums 164,128,160,55,167,61,42,65,66 -o 1.csv -d 1traj.pdb

/home/pb777/26/watpocket/build/src/watpocket/watpocket 0complex_wcn.pdb -d sal.py --resnums 164,128,160,55,167,61,42,65,66
/home/pb777/26/watpocket/build/src/watpocket/watpocket 0complex_wcn.pdb -d sal.pdb --resnums 164,128,160,55,167,61,42,65,66

# /home/pb777/26/watpocket/build/src/watpocket/watpocket 1complex_wcn.pdb -d sal.py --resnums 164,128,160,55,167,61,42,65,66
# /home/pb777/26/watpocket/build/src/watpocket/watpocket 1complex_wcn.pdb -d sal.pdb --resnums 164,128,160,55,167,61,42,65,66
