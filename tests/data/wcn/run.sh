#!/bin/bash

# /home/pb777/26/watpocket/build/src/watpocket/watpocket complex_wcn.parm7 complex_wcn_0.00000.nc --resnums 164,128,160,55,167,61,42,65,66 -o 0.csv
# /home/pb777/26/watpocket/build/src/watpocket/watpocket complex_wcn.parm7 complex_wcn_1.00000.nc --resnums 164,128,160,55,167,61,42,65,66 -o 1.csv

/home/pb777/26/watpocket/build/src/watpocket/watpocket complex_wcn.pdb -d sal.py --resnums 164,128,160,55,167,61,42,65,66
/home/pb777/26/watpocket/build/src/watpocket/watpocket complex_wcn.pdb -d sal.pdb --resnums 164,128,160,55,167,61,42,65,66
