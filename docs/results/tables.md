## HRV agreement with contact PPG

| dataset | config | metric | source | n | rPPG mean | reference mean | MAE | r |
|---|---|---|---|---|---|---|---|---|
| mcd | ablate-equal-prior | RMSSD | engine | 28 | 89.39 | 43.27 | 46.13 | 0.431 |
| mcd | ablate-equal-prior | SDNN | engine | 28 | 83.05 | 53.87 | 29.24 | 0.452 |
| mcd | ablate-equal-prior | meanHR | engine | 28 | 81.37 | 81.81 | 1.00 | 0.988 |
| mcd | ablate-equal-prior | pNN50 | engine | 28 | 54.42 | 18.98 | 35.43 | 0.260 |
| mcd | ablate-motion-comp-on | RMSSD | engine | 28 | 89.39 | 44.21 | 45.17 | 0.369 |
| mcd | ablate-motion-comp-on | SDNN | engine | 28 | 83.95 | 54.87 | 29.28 | 0.519 |
| mcd | ablate-motion-comp-on | meanHR | engine | 28 | 81.30 | 81.51 | 1.07 | 0.988 |
| mcd | ablate-motion-comp-on | pNN50 | engine | 28 | 54.14 | 20.04 | 34.11 | 0.318 |
| mcd | ablate-no-pose-prior | RMSSD | engine | 29 | 88.49 | 42.97 | 45.52 | 0.419 |
| mcd | ablate-no-pose-prior | SDNN | engine | 29 | 82.84 | 53.66 | 29.18 | 0.440 |
| mcd | ablate-no-pose-prior | meanHR | engine | 29 | 81.54 | 82.21 | 1.17 | 0.985 |
| mcd | ablate-no-pose-prior | pNN50 | engine | 29 | 54.11 | 18.79 | 35.32 | 0.267 |
| mcd | ablate-no-track | RMSSD | engine | 28 | 88.95 | 43.27 | 45.69 | 0.370 |
| mcd | ablate-no-track | SDNN | engine | 28 | 83.60 | 53.87 | 29.79 | 0.456 |
| mcd | ablate-no-track | meanHR | engine | 28 | 81.46 | 81.81 | 1.10 | 0.985 |
| mcd | ablate-no-track | pNN50 | engine | 28 | 54.15 | 18.98 | 35.16 | 0.213 |
| mcd | ablate-win15 | RMSSD | engine | 26 | 87.19 | 44.30 | 42.89 | 0.396 |
| mcd | ablate-win15 | SDNN | engine | 26 | 81.21 | 55.49 | 25.89 | 0.593 |
| mcd | ablate-win15 | meanHR | engine | 26 | 81.29 | 81.34 | 0.68 | 0.996 |
| mcd | ablate-win15 | pNN50 | engine | 26 | 53.67 | 19.94 | 33.73 | 0.390 |
| mcd | ablate-win30 | RMSSD | engine | 25 | 86.48 | 44.92 | 41.56 | 0.444 |
| mcd | ablate-win30 | SDNN | engine | 25 | 78.62 | 56.65 | 22.11 | 0.754 |
| mcd | ablate-win30 | meanHR | engine | 25 | 80.89 | 80.75 | 0.72 | 0.996 |
| mcd | ablate-win30 | pNN50 | engine | 25 | 53.54 | 20.52 | 33.02 | 0.395 |
| mcd | ablate-win8 | RMSSD | engine | 27 | 87.63 | 43.78 | 43.84 | 0.424 |
| mcd | ablate-win8 | SDNN | engine | 27 | 81.43 | 54.65 | 26.84 | 0.567 |
| mcd | ablate-win8 | meanHR | engine | 27 | 81.26 | 81.58 | 1.08 | 0.986 |
| mcd | ablate-win8 | pNN50 | engine | 27 | 54.15 | 19.45 | 34.70 | 0.307 |
| mcd | classical-only | RMSSD | engine | 14 | 107.29 | 49.91 | 57.38 | 0.626 |
| mcd | classical-only | SDNN | engine | 14 | 92.47 | 64.71 | 27.76 | 0.789 |
| mcd | classical-only | meanHR | engine | 14 | 75.89 | 75.59 | 0.88 | 0.994 |
| mcd | classical-only | pNN50 | engine | 14 | 63.50 | 23.64 | 39.86 | 0.491 |
| mcd | engine | RMSSD | engine | 28 | 88.95 | 43.27 | 45.69 | 0.370 |
| mcd | engine | SDNN | engine | 28 | 83.60 | 53.87 | 29.79 | 0.456 |
| mcd | engine | meanHR | engine | 28 | 81.46 | 81.81 | 1.10 | 0.985 |
| mcd | engine | pNN50 | engine | 28 | 54.15 | 18.98 | 35.16 | 0.213 |
| mcd | engine-lite | RMSSD | engine | 24 | 87.64 | 45.90 | 41.74 | 0.349 |
| mcd | engine-lite | SDNN | engine | 24 | 84.50 | 57.76 | 26.88 | 0.566 |
| mcd | engine-lite | meanHR | engine | 24 | 79.66 | 79.77 | 0.53 | 0.998 |
| mcd | engine-lite | pNN50 | engine | 24 | 53.31 | 21.29 | 32.02 | 0.347 |
| mcd | methods | RMSSD | engine | 26 | 91.51 | 43.46 | 48.05 | 0.418 |
| mcd | methods | SDNN | engine | 26 | 82.74 | 54.33 | 28.49 | 0.566 |
| mcd | methods | meanHR | engine | 26 | 81.25 | 81.56 | 0.86 | 0.995 |
| mcd | methods | pNN50 | engine | 26 | 56.40 | 19.01 | 37.39 | 0.137 |
| ubfc | ablate-equal-prior | RMSSD | engine | 38 | 36.40 | 47.85 | 14.26 | 0.754 |
| ubfc | ablate-equal-prior | SDNN | engine | 38 | 49.68 | 51.81 | 5.40 | 0.944 |
| ubfc | ablate-equal-prior | meanHR | engine | 38 | 98.04 | 97.91 | 0.35 | 1.000 |
| ubfc | ablate-equal-prior | pNN50 | engine | 38 | 14.90 | 24.05 | 11.55 | 0.733 |
| ubfc | ablate-motion-comp-on | RMSSD | engine | 38 | 34.98 | 47.85 | 14.69 | 0.797 |
| ubfc | ablate-motion-comp-on | SDNN | engine | 38 | 47.66 | 51.81 | 5.25 | 0.962 |
| ubfc | ablate-motion-comp-on | meanHR | engine | 38 | 98.12 | 97.91 | 0.34 | 1.000 |
| ubfc | ablate-motion-comp-on | pNN50 | engine | 38 | 14.41 | 24.05 | 11.43 | 0.785 |
| ubfc | ablate-no-pose-prior | RMSSD | engine | 38 | 34.84 | 47.85 | 14.68 | 0.805 |
| ubfc | ablate-no-pose-prior | SDNN | engine | 38 | 47.60 | 51.81 | 5.33 | 0.963 |
| ubfc | ablate-no-pose-prior | meanHR | engine | 38 | 98.11 | 97.91 | 0.34 | 1.000 |
| ubfc | ablate-no-pose-prior | pNN50 | engine | 38 | 14.25 | 24.05 | 11.48 | 0.791 |
| ubfc | ablate-no-track | RMSSD | engine | 38 | 34.99 | 47.85 | 14.68 | 0.798 |
| ubfc | ablate-no-track | SDNN | engine | 38 | 47.67 | 51.81 | 5.24 | 0.962 |
| ubfc | ablate-no-track | meanHR | engine | 38 | 98.12 | 97.91 | 0.34 | 1.000 |
| ubfc | ablate-no-track | pNN50 | engine | 38 | 14.39 | 24.05 | 11.46 | 0.785 |
| ubfc | ablate-win15 | RMSSD | engine | 38 | 35.09 | 47.85 | 14.44 | 0.800 |
| ubfc | ablate-win15 | SDNN | engine | 38 | 47.96 | 51.81 | 5.20 | 0.961 |
| ubfc | ablate-win15 | meanHR | engine | 38 | 98.09 | 97.91 | 0.31 | 1.000 |
| ubfc | ablate-win15 | pNN50 | engine | 38 | 14.58 | 24.05 | 11.18 | 0.776 |
| ubfc | ablate-win30 | RMSSD | engine | 38 | 35.47 | 47.85 | 14.35 | 0.783 |
| ubfc | ablate-win30 | SDNN | engine | 38 | 47.75 | 51.81 | 5.43 | 0.957 |
| ubfc | ablate-win30 | meanHR | engine | 38 | 98.08 | 97.91 | 0.30 | 1.000 |
| ubfc | ablate-win30 | pNN50 | engine | 38 | 14.49 | 24.05 | 11.14 | 0.800 |
| ubfc | ablate-win8 | RMSSD | engine | 38 | 35.20 | 47.85 | 14.54 | 0.792 |
| ubfc | ablate-win8 | SDNN | engine | 38 | 47.65 | 51.81 | 5.43 | 0.959 |
| ubfc | ablate-win8 | meanHR | engine | 38 | 98.11 | 97.91 | 0.33 | 1.000 |
| ubfc | ablate-win8 | pNN50 | engine | 38 | 14.45 | 24.05 | 11.28 | 0.782 |
| ubfc | classical-only | RMSSD | engine | 38 | 45.62 | 47.85 | 11.71 | 0.647 |
| ubfc | classical-only | SDNN | engine | 38 | 51.01 | 51.81 | 5.89 | 0.922 |
| ubfc | classical-only | meanHR | engine | 38 | 98.01 | 97.91 | 0.24 | 1.000 |
| ubfc | classical-only | pNN50 | engine | 38 | 23.84 | 24.05 | 10.07 | 0.609 |
| ubfc | engine | RMSSD | engine | 38 | 34.99 | 47.85 | 14.68 | 0.798 |
| ubfc | engine | SDNN | engine | 38 | 47.67 | 51.81 | 5.24 | 0.962 |
| ubfc | engine | meanHR | engine | 38 | 98.12 | 97.91 | 0.34 | 1.000 |
| ubfc | engine | pNN50 | engine | 38 | 14.39 | 24.05 | 11.46 | 0.785 |
| ubfc | engine-lite | RMSSD | engine | 38 | 35.85 | 47.85 | 14.11 | 0.777 |
| ubfc | engine-lite | SDNN | engine | 38 | 48.03 | 51.81 | 5.15 | 0.957 |
| ubfc | engine-lite | meanHR | engine | 38 | 98.05 | 97.91 | 0.29 | 1.000 |
| ubfc | engine-lite | pNN50 | engine | 38 | 14.81 | 24.05 | 10.89 | 0.768 |
| ubfc | methods | RMSSD | engine | 38 | 40.81 | 47.85 | 11.91 | 0.741 |
| ubfc | methods | SDNN | engine | 38 | 50.75 | 51.81 | 5.09 | 0.952 |
| ubfc | methods | meanHR | engine | 38 | 97.94 | 97.91 | 0.29 | 1.000 |
| ubfc | methods | pNN50 | engine | 38 | 19.41 | 24.05 | 9.67 | 0.697 |

## Protocol: whole

| dataset | config | candidate | n | MAE | RMSE | MAPE | r | acc3 | acc5 |
|---|---|---|---|---|---|---|---|---|---|
| mcd | methods | ME-rPPG | 44.00 | 2.18 | 3.92 | 2.64 | 0.950 | 70.5 | 84.1 |
| mcd | engine | ME-rPPG | 44.00 | 2.18 | 3.92 | 2.64 | 0.950 | 70.5 | 84.1 |
| mcd | ablate-no-track | ME-rPPG | 44.00 | 2.18 | 3.92 | 2.64 | 0.950 | 70.5 | 84.1 |
| mcd | ablate-motion-comp-on | ME-rPPG | 44.00 | 2.18 | 3.92 | 2.64 | 0.950 | 70.5 | 84.1 |
| mcd | ablate-no-pose-prior | ME-rPPG | 44.00 | 2.18 | 3.92 | 2.64 | 0.950 | 70.5 | 84.1 |
| mcd | ablate-equal-prior | ME-rPPG | 44.00 | 2.18 | 3.92 | 2.64 | 0.950 | 70.5 | 84.1 |
| mcd | ablate-win8 | ME-rPPG | 44.00 | 2.18 | 3.92 | 2.64 | 0.950 | 70.5 | 84.1 |
| mcd | ablate-win15 | ME-rPPG | 44.00 | 2.18 | 3.92 | 2.64 | 0.950 | 70.5 | 84.1 |
| mcd | ablate-win30 | ME-rPPG | 44.00 | 2.18 | 3.92 | 2.64 | 0.950 | 70.5 | 84.1 |
| mcd | ablate-no-pose-prior | FUSED | 44.00 | 2.37 | 5.70 | 2.67 | 0.898 | 81.8 | 90.9 |
| mcd | ablate-win15 | FUSED | 44.00 | 2.52 | 5.79 | 2.88 | 0.896 | 77.3 | 88.6 |
| mcd | ablate-win30 | FUSED | 44.00 | 2.61 | 5.93 | 3.00 | 0.888 | 79.5 | 86.4 |
| mcd | ablate-equal-prior | FUSED | 44.00 | 2.86 | 6.46 | 3.20 | 0.879 | 77.3 | 88.6 |
| mcd | engine | FUSED | 44.00 | 2.95 | 6.51 | 3.30 | 0.876 | 75.0 | 86.4 |
| mcd | ablate-no-track | FUSED | 44.00 | 2.95 | 6.51 | 3.30 | 0.876 | 75.0 | 86.4 |
| mcd | ablate-motion-comp-on | FUSED | 44.00 | 2.96 | 6.50 | 3.32 | 0.874 | 75.0 | 86.4 |
| mcd | ablate-win8 | FUSED | 44.00 | 3.06 | 6.56 | 3.44 | 0.873 | 72.7 | 84.1 |
| mcd | methods | FUSED | 44.00 | 3.16 | 6.75 | 3.57 | 0.875 | 75.0 | 86.4 |
| mcd | methods | FacePhys | 44.00 | 3.92 | 9.28 | 4.42 | 0.760 | 75.0 | 84.1 |
| mcd | engine | FacePhys | 44.00 | 3.92 | 9.28 | 4.42 | 0.760 | 75.0 | 84.1 |
| mcd | engine-lite | FacePhys | 44.00 | 3.92 | 9.28 | 4.42 | 0.760 | 75.0 | 84.1 |
| mcd | ablate-no-track | FacePhys | 44.00 | 3.92 | 9.28 | 4.42 | 0.760 | 75.0 | 84.1 |
| mcd | ablate-motion-comp-on | FacePhys | 44.00 | 3.92 | 9.28 | 4.42 | 0.760 | 75.0 | 84.1 |
| mcd | ablate-no-pose-prior | FacePhys | 44.00 | 3.92 | 9.28 | 4.42 | 0.760 | 75.0 | 84.1 |
| mcd | ablate-equal-prior | FacePhys | 44.00 | 3.92 | 9.28 | 4.42 | 0.760 | 75.0 | 84.1 |
| mcd | ablate-win8 | FacePhys | 44.00 | 3.92 | 9.28 | 4.42 | 0.760 | 75.0 | 84.1 |
| mcd | ablate-win15 | FacePhys | 44.00 | 3.92 | 9.28 | 4.42 | 0.760 | 75.0 | 84.1 |
| mcd | ablate-win30 | FacePhys | 44.00 | 3.92 | 9.28 | 4.42 | 0.760 | 75.0 | 84.1 |
| mcd | engine-lite | FUSED | 44.00 | 4.17 | 9.33 | 4.70 | 0.759 | 68.2 | 81.8 |
| mcd | methods | LGI | 44.00 | 9.24 | 16.49 | 9.76 | 0.321 | 61.4 | 65.9 |
| mcd | methods | OMIT | 44.00 | 9.27 | 16.50 | 9.81 | 0.322 | 61.4 | 65.9 |
| mcd | methods | GREEN | 44.00 | 10.72 | 18.25 | 11.77 | 0.316 | 50.0 | 59.1 |
| mcd | classical-only | FUSED | 44.00 | 11.06 | 19.03 | 11.84 | 0.220 | 52.3 | 65.9 |
| mcd | ablate-win8 | POS-MR | 44.00 | 11.21 | 19.84 | 12.27 | 0.338 | 59.1 | 65.9 |
| mcd | methods | POS-MR | 44.00 | 11.21 | 19.84 | 12.27 | 0.338 | 59.1 | 65.9 |
| mcd | engine | POS-MR | 44.00 | 11.21 | 19.84 | 12.27 | 0.338 | 59.1 | 65.9 |
| mcd | ablate-no-track | POS-MR | 44.00 | 11.21 | 19.84 | 12.27 | 0.338 | 59.1 | 65.9 |
| mcd | ablate-motion-comp-on | POS-MR | 44.00 | 11.21 | 19.84 | 12.27 | 0.338 | 59.1 | 65.9 |
| mcd | ablate-win15 | POS-MR | 44.00 | 11.21 | 19.84 | 12.27 | 0.338 | 59.1 | 65.9 |
| mcd | ablate-equal-prior | POS-MR | 44.00 | 11.21 | 19.84 | 12.27 | 0.338 | 59.1 | 65.9 |
| mcd | engine-lite | POS-MR | 44.00 | 11.21 | 19.84 | 12.27 | 0.338 | 59.1 | 65.9 |
| mcd | ablate-win30 | POS-MR | 44.00 | 11.21 | 19.84 | 12.27 | 0.338 | 59.1 | 65.9 |
| mcd | ablate-no-pose-prior | POS-MR | 44.00 | 11.76 | 20.23 | 12.78 | 0.291 | 56.8 | 63.6 |
| mcd | methods | PBV | 44.00 | 12.18 | 21.31 | 13.04 | 0.116 | 59.1 | 65.9 |
| mcd | classical-only | POS-MR | 44.00 | 12.23 | 21.06 | 13.23 | 0.241 | 56.8 | 63.6 |
| mcd | methods | POS | 44.00 | 13.02 | 23.34 | 14.13 | 0.134 | 56.8 | 61.4 |
| mcd | engine | POS | 44.00 | 13.02 | 23.34 | 14.13 | 0.134 | 56.8 | 61.4 |
| mcd | engine-lite | POS | 44.00 | 13.02 | 23.34 | 14.13 | 0.134 | 56.8 | 61.4 |
| mcd | classical-only | POS | 44.00 | 13.02 | 23.34 | 14.13 | 0.134 | 56.8 | 61.4 |
| mcd | ablate-no-track | POS | 44.00 | 13.02 | 23.34 | 14.13 | 0.134 | 56.8 | 61.4 |
| mcd | ablate-motion-comp-on | POS | 44.00 | 13.02 | 23.34 | 14.13 | 0.134 | 56.8 | 61.4 |
| mcd | ablate-no-pose-prior | POS | 44.00 | 13.02 | 23.34 | 14.13 | 0.134 | 56.8 | 61.4 |
| mcd | ablate-equal-prior | POS | 44.00 | 13.02 | 23.34 | 14.13 | 0.134 | 56.8 | 61.4 |
| mcd | ablate-win8 | POS | 44.00 | 13.02 | 23.34 | 14.13 | 0.134 | 56.8 | 61.4 |
| mcd | ablate-win15 | POS | 44.00 | 13.02 | 23.34 | 14.13 | 0.134 | 56.8 | 61.4 |
| mcd | ablate-win30 | POS | 44.00 | 13.02 | 23.34 | 14.13 | 0.134 | 56.8 | 61.4 |
| mcd | methods | 2SR | 44.00 | 16.22 | 25.03 | 17.38 | -0.011 | 47.7 | 54.5 |
| mcd | methods | PRISM | 44.00 | 17.09 | 28.17 | 19.47 | 0.103 | 40.9 | 50.0 |
| mcd | methods | PCA | 44.00 | 19.47 | 27.05 | 21.30 | -0.209 | 34.1 | 40.9 |
| mcd | methods | CHROM | 44.00 | 20.07 | 29.21 | 22.15 | -0.073 | 45.5 | 45.5 |
| mcd | methods | ICA | 44.00 | 20.41 | 28.06 | 22.25 | -0.252 | 43.2 | 43.2 |
| ubfc | ablate-equal-prior | POS-MR | 38.00 | 0.46 | 1.56 | 0.48 | 0.997 | 94.7 | 97.4 |
| ubfc | methods | POS-MR | 38.00 | 0.46 | 1.56 | 0.48 | 0.997 | 94.7 | 97.4 |
| ubfc | engine | POS-MR | 38.00 | 0.46 | 1.56 | 0.48 | 0.997 | 94.7 | 97.4 |
| ubfc | ablate-no-track | POS-MR | 38.00 | 0.46 | 1.56 | 0.48 | 0.997 | 94.7 | 97.4 |
| ubfc | ablate-motion-comp-on | POS-MR | 38.00 | 0.46 | 1.56 | 0.48 | 0.997 | 94.7 | 97.4 |
| ubfc | ablate-no-pose-prior | POS-MR | 38.00 | 0.46 | 1.56 | 0.48 | 0.997 | 94.7 | 97.4 |
| ubfc | ablate-win8 | POS-MR | 38.00 | 0.46 | 1.56 | 0.48 | 0.997 | 94.7 | 97.4 |
| ubfc | classical-only | POS-MR | 38.00 | 0.46 | 1.56 | 0.48 | 0.997 | 94.7 | 97.4 |
| ubfc | engine-lite | POS-MR | 38.00 | 0.46 | 1.56 | 0.48 | 0.997 | 94.7 | 97.4 |
| ubfc | ablate-win30 | POS-MR | 38.00 | 0.46 | 1.56 | 0.48 | 0.997 | 94.7 | 97.4 |
| ubfc | ablate-win15 | POS-MR | 38.00 | 0.46 | 1.56 | 0.48 | 0.997 | 94.7 | 97.4 |
| ubfc | methods | PRISM | 38.00 | 0.71 | 2.20 | 0.70 | 0.993 | 92.1 | 94.7 |
| ubfc | methods | ME-rPPG | 38.00 | 0.72 | 2.12 | 0.77 | 0.993 | 92.1 | 94.7 |
| ubfc | engine | ME-rPPG | 38.00 | 0.72 | 2.12 | 0.77 | 0.993 | 92.1 | 94.7 |
| ubfc | ablate-no-track | ME-rPPG | 38.00 | 0.72 | 2.12 | 0.77 | 0.993 | 92.1 | 94.7 |
| ubfc | ablate-motion-comp-on | ME-rPPG | 38.00 | 0.72 | 2.12 | 0.77 | 0.993 | 92.1 | 94.7 |
| ubfc | ablate-no-pose-prior | ME-rPPG | 38.00 | 0.72 | 2.12 | 0.77 | 0.993 | 92.1 | 94.7 |
| ubfc | ablate-equal-prior | ME-rPPG | 38.00 | 0.72 | 2.12 | 0.77 | 0.993 | 92.1 | 94.7 |
| ubfc | ablate-win8 | ME-rPPG | 38.00 | 0.72 | 2.12 | 0.77 | 0.993 | 92.1 | 94.7 |
| ubfc | ablate-win15 | ME-rPPG | 38.00 | 0.72 | 2.12 | 0.77 | 0.993 | 92.1 | 94.7 |
| ubfc | ablate-win30 | ME-rPPG | 38.00 | 0.72 | 2.12 | 0.77 | 0.993 | 92.1 | 94.7 |
| ubfc | methods | POS | 38.00 | 0.94 | 2.62 | 0.93 | 0.990 | 86.8 | 92.1 |
| ubfc | engine | POS | 38.00 | 0.94 | 2.62 | 0.93 | 0.990 | 86.8 | 92.1 |
| ubfc | engine-lite | POS | 38.00 | 0.94 | 2.62 | 0.93 | 0.990 | 86.8 | 92.1 |
| ubfc | classical-only | POS | 38.00 | 0.94 | 2.62 | 0.93 | 0.990 | 86.8 | 92.1 |
| ubfc | ablate-no-track | POS | 38.00 | 0.94 | 2.62 | 0.93 | 0.990 | 86.8 | 92.1 |
| ubfc | ablate-motion-comp-on | POS | 38.00 | 0.94 | 2.62 | 0.93 | 0.990 | 86.8 | 92.1 |
| ubfc | ablate-no-pose-prior | POS | 38.00 | 0.94 | 2.62 | 0.93 | 0.990 | 86.8 | 92.1 |
| ubfc | ablate-equal-prior | POS | 38.00 | 0.94 | 2.62 | 0.93 | 0.990 | 86.8 | 92.1 |
| ubfc | ablate-win8 | POS | 38.00 | 0.94 | 2.62 | 0.93 | 0.990 | 86.8 | 92.1 |
| ubfc | ablate-win15 | POS | 38.00 | 0.94 | 2.62 | 0.93 | 0.990 | 86.8 | 92.1 |
| ubfc | ablate-win30 | POS | 38.00 | 0.94 | 2.62 | 0.93 | 0.990 | 86.8 | 92.1 |
| ubfc | methods | FUSED | 38.00 | 0.95 | 1.76 | 1.04 | 0.995 | 86.8 | 97.4 |
| ubfc | methods | FacePhys | 38.00 | 0.98 | 3.55 | 0.92 | 0.983 | 92.1 | 94.7 |
| ubfc | engine | FacePhys | 38.00 | 0.98 | 3.55 | 0.92 | 0.983 | 92.1 | 94.7 |
| ubfc | engine-lite | FacePhys | 38.00 | 0.98 | 3.55 | 0.92 | 0.983 | 92.1 | 94.7 |
| ubfc | ablate-no-track | FacePhys | 38.00 | 0.98 | 3.55 | 0.92 | 0.983 | 92.1 | 94.7 |
| ubfc | ablate-motion-comp-on | FacePhys | 38.00 | 0.98 | 3.55 | 0.92 | 0.983 | 92.1 | 94.7 |
| ubfc | ablate-no-pose-prior | FacePhys | 38.00 | 0.98 | 3.55 | 0.92 | 0.983 | 92.1 | 94.7 |
| ubfc | ablate-equal-prior | FacePhys | 38.00 | 0.98 | 3.55 | 0.92 | 0.983 | 92.1 | 94.7 |
| ubfc | ablate-win8 | FacePhys | 38.00 | 0.98 | 3.55 | 0.92 | 0.983 | 92.1 | 94.7 |
| ubfc | ablate-win15 | FacePhys | 38.00 | 0.98 | 3.55 | 0.92 | 0.983 | 92.1 | 94.7 |
| ubfc | ablate-win30 | FacePhys | 38.00 | 0.98 | 3.55 | 0.92 | 0.983 | 92.1 | 94.7 |
| ubfc | engine-lite | FUSED | 38.00 | 0.99 | 3.60 | 0.93 | 0.982 | 92.1 | 94.7 |
| ubfc | ablate-win8 | FUSED | 38.00 | 1.00 | 3.59 | 0.94 | 0.983 | 92.1 | 94.7 |
| ubfc | ablate-win30 | FUSED | 38.00 | 1.00 | 3.57 | 0.94 | 0.983 | 92.1 | 94.7 |
| ubfc | ablate-equal-prior | FUSED | 38.00 | 1.02 | 2.59 | 1.03 | 0.990 | 89.5 | 97.4 |
| ubfc | classical-only | FUSED | 38.00 | 1.05 | 3.60 | 1.03 | 0.982 | 92.1 | 94.7 |
| ubfc | ablate-win15 | FUSED | 38.00 | 1.06 | 3.59 | 1.01 | 0.983 | 92.1 | 94.7 |
| ubfc | ablate-motion-comp-on | FUSED | 38.00 | 1.11 | 3.64 | 1.05 | 0.982 | 89.5 | 94.7 |
| ubfc | engine | FUSED | 38.00 | 1.11 | 3.64 | 1.05 | 0.982 | 89.5 | 94.7 |
| ubfc | ablate-no-track | FUSED | 38.00 | 1.11 | 3.64 | 1.05 | 0.982 | 89.5 | 94.7 |
| ubfc | ablate-no-pose-prior | FUSED | 38.00 | 1.11 | 3.65 | 1.05 | 0.982 | 89.5 | 94.7 |
| ubfc | methods | GREEN | 38.00 | 1.21 | 2.42 | 1.21 | 0.991 | 84.2 | 92.1 |
| ubfc | methods | OMIT | 38.00 | 1.27 | 3.70 | 1.30 | 0.982 | 89.5 | 94.7 |
| ubfc | methods | LGI | 38.00 | 1.27 | 3.70 | 1.30 | 0.982 | 89.5 | 94.7 |
| ubfc | methods | CHROM | 38.00 | 1.82 | 4.41 | 1.80 | 0.974 | 81.6 | 89.5 |
| ubfc | methods | PBV | 38.00 | 1.91 | 4.42 | 1.92 | 0.974 | 81.6 | 86.8 |
| ubfc | methods | PCA | 38.00 | 2.17 | 3.33 | 2.09 | 0.984 | 78.9 | 84.2 |
| ubfc | methods | 2SR | 38.00 | 2.28 | 7.11 | 2.35 | 0.939 | 84.2 | 89.5 |
| ubfc | methods | ICA | 38.00 | 3.24 | 8.27 | 3.30 | 0.912 | 71.1 | 89.5 |

## Protocol: window

| dataset | config | candidate | n | MAE | RMSE | MAPE | r | acc3 | acc5 |
|---|---|---|---|---|---|---|---|---|---|
| mcd | ablate-win30 | FUSED | 6532.00 | 3.30 | 7.79 | 3.69 | 0.825 | 76.0 | 82.3 |
| mcd | ablate-win30 | ME-rPPG | 6532.00 | 4.26 | 9.12 | 4.86 | 0.765 | 71.3 | 77.0 |
| mcd | ablate-win30 | FUSED_RAW | 6532.00 | 4.26 | 9.49 | 4.78 | 0.753 | 73.3 | 78.7 |
| mcd | ablate-win15 | FUSED | 7192.00 | 4.28 | 9.32 | 4.81 | 0.766 | 75.1 | 78.6 |
| mcd | engine | FUSED | 7412.00 | 4.93 | 10.22 | 5.60 | 0.726 | 71.8 | 77.7 |
| mcd | ablate-equal-prior | FUSED | 7412.00 | 5.00 | 10.34 | 5.66 | 0.722 | 71.6 | 77.2 |
| mcd | ablate-motion-comp-on | FUSED | 7412.00 | 5.07 | 10.17 | 5.86 | 0.726 | 68.7 | 76.7 |
| mcd | ablate-win8 | FUSED | 7500.00 | 5.24 | 10.42 | 5.98 | 0.716 | 68.0 | 76.1 |
| mcd | ablate-win15 | FUSED_RAW | 7192.00 | 5.37 | 11.35 | 6.05 | 0.681 | 72.0 | 75.1 |
| mcd | ablate-win30 | FacePhys | 6521.00 | 5.40 | 11.65 | 6.11 | 0.666 | 68.8 | 74.7 |
| mcd | ablate-win15 | ME-rPPG | 7192.00 | 5.52 | 11.35 | 6.31 | 0.672 | 69.9 | 73.7 |
| mcd | ablate-no-pose-prior | FUSED | 7412.00 | 5.72 | 11.60 | 6.38 | 0.664 | 70.0 | 75.3 |
| mcd | engine-lite | FUSED | 7412.00 | 5.99 | 11.92 | 6.73 | 0.659 | 67.0 | 73.1 |
| mcd | methods | FUSED | 7412.00 | 6.27 | 12.73 | 7.05 | 0.623 | 68.6 | 73.9 |
| mcd | engine | FUSED_RAW | 7412.00 | 6.43 | 12.76 | 7.29 | 0.619 | 68.1 | 72.9 |
| mcd | ablate-no-track | FUSED | 7412.00 | 6.43 | 12.76 | 7.29 | 0.619 | 68.1 | 72.9 |
| mcd | ablate-no-track | FUSED_RAW | 7412.00 | 6.43 | 12.76 | 7.29 | 0.619 | 68.1 | 72.9 |
| mcd | ablate-equal-prior | FUSED_RAW | 7412.00 | 6.45 | 12.77 | 7.29 | 0.620 | 67.9 | 72.7 |
| mcd | methods | ME-rPPG | 7412.00 | 6.55 | 12.84 | 7.56 | 0.610 | 66.2 | 71.7 |
| mcd | engine | ME-rPPG | 7412.00 | 6.55 | 12.84 | 7.56 | 0.610 | 66.2 | 71.7 |
| mcd | ablate-no-track | ME-rPPG | 7412.00 | 6.55 | 12.84 | 7.56 | 0.610 | 66.2 | 71.7 |
| mcd | ablate-motion-comp-on | ME-rPPG | 7412.00 | 6.55 | 12.84 | 7.56 | 0.610 | 66.2 | 71.7 |
| mcd | ablate-no-pose-prior | ME-rPPG | 7412.00 | 6.55 | 12.84 | 7.56 | 0.610 | 66.2 | 71.7 |
| mcd | ablate-equal-prior | ME-rPPG | 7412.00 | 6.55 | 12.84 | 7.56 | 0.610 | 66.2 | 71.7 |
| mcd | ablate-win15 | FacePhys | 7166.00 | 6.94 | 14.12 | 7.91 | 0.588 | 66.6 | 70.3 |
| mcd | ablate-win8 | FUSED_RAW | 7500.00 | 7.02 | 13.36 | 8.00 | 0.589 | 64.1 | 71.0 |
| mcd | ablate-motion-comp-on | FUSED_RAW | 7412.00 | 7.09 | 14.00 | 8.16 | 0.592 | 64.6 | 71.5 |
| mcd | ablate-no-pose-prior | FUSED_RAW | 7412.00 | 7.18 | 14.05 | 8.00 | 0.551 | 66.8 | 71.4 |
| mcd | ablate-win8 | ME-rPPG | 7500.00 | 7.18 | 13.48 | 8.30 | 0.578 | 62.0 | 69.8 |
| mcd | methods | FacePhys | 7381.00 | 8.22 | 15.83 | 9.39 | 0.535 | 62.4 | 67.9 |
| mcd | engine | FacePhys | 7381.00 | 8.22 | 15.83 | 9.39 | 0.535 | 62.4 | 67.9 |
| mcd | engine-lite | FacePhys | 7381.00 | 8.22 | 15.83 | 9.39 | 0.535 | 62.4 | 67.9 |
| mcd | ablate-no-track | FacePhys | 7381.00 | 8.22 | 15.83 | 9.39 | 0.535 | 62.4 | 67.9 |
| mcd | ablate-motion-comp-on | FacePhys | 7381.00 | 8.22 | 15.83 | 9.39 | 0.535 | 62.4 | 67.9 |
| mcd | ablate-no-pose-prior | FacePhys | 7381.00 | 8.22 | 15.83 | 9.39 | 0.535 | 62.4 | 67.9 |
| mcd | ablate-equal-prior | FacePhys | 7381.00 | 8.22 | 15.83 | 9.39 | 0.535 | 62.4 | 67.9 |
| mcd | engine-lite | FUSED_RAW | 7412.00 | 8.26 | 15.59 | 9.37 | 0.529 | 62.3 | 67.5 |
| mcd | methods | FUSED_RAW | 7412.00 | 8.41 | 15.91 | 9.44 | 0.488 | 64.1 | 68.4 |
| mcd | ablate-win8 | FacePhys | 7467.00 | 8.84 | 16.52 | 10.15 | 0.511 | 58.6 | 66.1 |
| mcd | ablate-win30 | POS | 6532.00 | 11.72 | 19.96 | 12.83 | 0.284 | 53.0 | 57.7 |
| mcd | ablate-win15 | POS | 7192.00 | 12.52 | 20.58 | 13.84 | 0.264 | 50.9 | 54.1 |
| mcd | ablate-win30 | POS-MR | 6532.00 | 12.57 | 20.02 | 13.87 | 0.265 | 47.7 | 52.1 |
| mcd | classical-only | FUSED | 7412.00 | 12.85 | 19.90 | 13.98 | 0.214 | 47.1 | 52.3 |
| mcd | methods | POS | 7412.00 | 13.09 | 20.91 | 14.59 | 0.256 | 48.4 | 53.7 |
| mcd | engine | POS | 7412.00 | 13.09 | 20.91 | 14.59 | 0.256 | 48.4 | 53.7 |
| mcd | engine-lite | POS | 7412.00 | 13.09 | 20.91 | 14.59 | 0.256 | 48.4 | 53.7 |
| mcd | classical-only | POS | 7412.00 | 13.09 | 20.91 | 14.59 | 0.256 | 48.4 | 53.7 |
| mcd | ablate-no-track | POS | 7412.00 | 13.09 | 20.91 | 14.59 | 0.256 | 48.4 | 53.7 |
| mcd | ablate-motion-comp-on | POS | 7412.00 | 13.09 | 20.91 | 14.59 | 0.256 | 48.4 | 53.7 |
| mcd | ablate-no-pose-prior | POS | 7412.00 | 13.09 | 20.91 | 14.59 | 0.256 | 48.4 | 53.7 |
| mcd | ablate-equal-prior | POS | 7412.00 | 13.09 | 20.91 | 14.59 | 0.256 | 48.4 | 53.7 |
| mcd | ablate-win8 | POS | 7500.00 | 13.57 | 21.32 | 15.21 | 0.247 | 44.7 | 52.1 |
| mcd | classical-only | FUSED_RAW | 7412.00 | 13.73 | 21.11 | 15.23 | 0.203 | 45.0 | 49.9 |
| mcd | ablate-win15 | POS-MR | 7192.00 | 13.91 | 21.05 | 15.36 | 0.203 | 43.8 | 47.1 |
| mcd | methods | OMIT | 7412.00 | 14.04 | 22.11 | 15.54 | 0.187 | 47.5 | 52.3 |
| mcd | methods | LGI | 7412.00 | 14.09 | 22.16 | 15.59 | 0.186 | 47.5 | 52.2 |
| mcd | methods | PBV | 7412.00 | 14.45 | 21.89 | 16.15 | 0.222 | 42.8 | 48.4 |
| mcd | methods | POS-MR | 7412.00 | 14.51 | 21.49 | 16.17 | 0.184 | 40.3 | 45.5 |
| mcd | engine | POS-MR | 7412.00 | 14.51 | 21.49 | 16.17 | 0.184 | 40.3 | 45.5 |
| mcd | engine-lite | POS-MR | 7412.00 | 14.51 | 21.49 | 16.17 | 0.184 | 40.3 | 45.5 |
| mcd | classical-only | POS-MR | 7412.00 | 14.51 | 21.49 | 16.17 | 0.184 | 40.3 | 45.5 |
| mcd | ablate-no-track | POS-MR | 7412.00 | 14.51 | 21.49 | 16.17 | 0.184 | 40.3 | 45.5 |
| mcd | ablate-motion-comp-on | POS-MR | 7412.00 | 14.51 | 21.49 | 16.17 | 0.184 | 40.3 | 45.5 |
| mcd | ablate-no-pose-prior | POS-MR | 7412.00 | 14.51 | 21.49 | 16.17 | 0.184 | 40.3 | 45.5 |
| mcd | ablate-equal-prior | POS-MR | 7412.00 | 14.51 | 21.49 | 16.17 | 0.184 | 40.3 | 45.5 |
| mcd | methods | 2SR | 7412.00 | 14.64 | 22.73 | 16.24 | 0.186 | 46.4 | 51.2 |
| mcd | ablate-win8 | POS-MR | 7500.00 | 14.86 | 21.72 | 16.62 | 0.173 | 37.2 | 44.3 |
| mcd | methods | GREEN | 7412.00 | 15.96 | 23.28 | 17.74 | 0.148 | 40.2 | 44.5 |
| mcd | methods | PRISM | 7412.00 | 16.65 | 25.91 | 19.13 | 0.140 | 39.8 | 45.1 |
| mcd | methods | CHROM | 7412.00 | 18.22 | 25.81 | 20.55 | 0.149 | 35.4 | 40.2 |
| mcd | methods | ICA | 7412.00 | 20.83 | 27.78 | 23.32 | 0.039 | 23.5 | 29.7 |
| mcd | methods | PCA | 7412.00 | 20.92 | 27.65 | 23.43 | 0.050 | 21.9 | 29.0 |
| ubfc | engine-lite | FUSED_RAW | 2155.00 | 0.50 | 1.32 | 0.53 | 0.997 | 98.8 | 99.1 |
| ubfc | ablate-win15 | FUSED_RAW | 1965.00 | 0.51 | 1.64 | 0.56 | 0.996 | 97.4 | 97.9 |
| ubfc | ablate-equal-prior | FUSED_RAW | 2155.00 | 0.52 | 1.39 | 0.55 | 0.997 | 98.8 | 99.0 |
| ubfc | engine | FUSED_RAW | 2155.00 | 0.54 | 1.45 | 0.57 | 0.997 | 98.8 | 99.0 |
| ubfc | ablate-no-track | FUSED | 2155.00 | 0.54 | 1.45 | 0.57 | 0.997 | 98.8 | 99.0 |
| ubfc | ablate-no-track | FUSED_RAW | 2155.00 | 0.54 | 1.45 | 0.57 | 0.997 | 98.8 | 99.0 |
| ubfc | ablate-no-pose-prior | FUSED_RAW | 2155.00 | 0.54 | 1.48 | 0.57 | 0.997 | 98.8 | 99.0 |
| ubfc | methods | FUSED_RAW | 2155.00 | 0.54 | 1.72 | 0.57 | 0.995 | 98.7 | 98.9 |
| ubfc | ablate-win30 | FUSED_RAW | 1395.00 | 0.56 | 1.77 | 0.57 | 0.996 | 94.6 | 97.3 |
| ubfc | ablate-equal-prior | FUSED | 2155.00 | 0.56 | 1.61 | 0.58 | 0.996 | 98.5 | 98.8 |
| ubfc | ablate-win30 | FUSED | 1395.00 | 0.56 | 1.86 | 0.58 | 0.995 | 94.7 | 97.3 |
| ubfc | engine-lite | FUSED | 2155.00 | 0.56 | 1.68 | 0.58 | 0.996 | 98.3 | 98.7 |
| ubfc | ablate-win15 | FacePhys | 1965.00 | 0.57 | 1.92 | 0.63 | 0.994 | 97.2 | 97.5 |
| ubfc | ablate-win15 | POS-MR | 1965.00 | 0.57 | 1.85 | 0.62 | 0.995 | 97.2 | 97.6 |
| ubfc | ablate-no-pose-prior | FUSED | 2155.00 | 0.57 | 1.62 | 0.60 | 0.996 | 98.5 | 98.8 |
| ubfc | engine | FUSED | 2155.00 | 0.57 | 1.63 | 0.60 | 0.996 | 98.5 | 98.8 |
| ubfc | ablate-win30 | POS-MR | 1395.00 | 0.57 | 1.80 | 0.59 | 0.995 | 94.4 | 96.8 |
| ubfc | ablate-win30 | FacePhys | 1395.00 | 0.57 | 1.94 | 0.60 | 0.995 | 94.6 | 97.1 |
| ubfc | ablate-motion-comp-on | FUSED_RAW | 2155.00 | 0.58 | 1.51 | 0.61 | 0.997 | 98.7 | 99.0 |
| ubfc | methods | FUSED | 2155.00 | 0.58 | 1.78 | 0.61 | 0.995 | 98.3 | 98.6 |
| ubfc | ablate-motion-comp-on | FUSED | 2155.00 | 0.61 | 1.65 | 0.63 | 0.996 | 98.4 | 98.8 |
| ubfc | classical-only | FUSED | 2155.00 | 0.62 | 1.76 | 0.65 | 0.995 | 98.1 | 98.7 |
| ubfc | methods | FacePhys | 2155.00 | 0.62 | 2.03 | 0.67 | 0.994 | 98.4 | 98.6 |
| ubfc | engine | FacePhys | 2155.00 | 0.62 | 2.03 | 0.67 | 0.994 | 98.4 | 98.6 |
| ubfc | engine-lite | FacePhys | 2155.00 | 0.62 | 2.03 | 0.67 | 0.994 | 98.4 | 98.6 |
| ubfc | ablate-no-track | FacePhys | 2155.00 | 0.62 | 2.03 | 0.67 | 0.994 | 98.4 | 98.6 |
| ubfc | ablate-motion-comp-on | FacePhys | 2155.00 | 0.62 | 2.03 | 0.67 | 0.994 | 98.4 | 98.6 |
| ubfc | ablate-no-pose-prior | FacePhys | 2155.00 | 0.62 | 2.03 | 0.67 | 0.994 | 98.4 | 98.6 |
| ubfc | ablate-equal-prior | FacePhys | 2155.00 | 0.62 | 2.03 | 0.67 | 0.994 | 98.4 | 98.6 |
| ubfc | methods | POS-MR | 2155.00 | 0.63 | 1.96 | 0.67 | 0.994 | 98.1 | 98.7 |
| ubfc | engine | POS-MR | 2155.00 | 0.63 | 1.96 | 0.67 | 0.994 | 98.1 | 98.7 |
| ubfc | engine-lite | POS-MR | 2155.00 | 0.63 | 1.96 | 0.67 | 0.994 | 98.1 | 98.7 |
| ubfc | classical-only | POS-MR | 2155.00 | 0.63 | 1.96 | 0.67 | 0.994 | 98.1 | 98.7 |
| ubfc | ablate-no-track | POS-MR | 2155.00 | 0.63 | 1.96 | 0.67 | 0.994 | 98.1 | 98.7 |
| ubfc | ablate-motion-comp-on | POS-MR | 2155.00 | 0.63 | 1.96 | 0.67 | 0.994 | 98.1 | 98.7 |
| ubfc | ablate-no-pose-prior | POS-MR | 2155.00 | 0.63 | 1.96 | 0.67 | 0.994 | 98.1 | 98.7 |
| ubfc | ablate-equal-prior | POS-MR | 2155.00 | 0.63 | 1.96 | 0.67 | 0.994 | 98.1 | 98.7 |
| ubfc | ablate-win8 | FUSED_RAW | 2231.00 | 0.63 | 1.55 | 0.66 | 0.996 | 98.8 | 99.2 |
| ubfc | classical-only | FUSED_RAW | 2155.00 | 0.63 | 2.09 | 0.68 | 0.993 | 98.1 | 98.7 |
| ubfc | ablate-win8 | FUSED | 2231.00 | 0.64 | 1.54 | 0.67 | 0.996 | 98.7 | 99.1 |
| ubfc | ablate-win15 | FUSED | 1965.00 | 0.65 | 2.19 | 0.73 | 0.993 | 96.5 | 97.0 |
| ubfc | ablate-win30 | POS | 1395.00 | 0.66 | 2.03 | 0.67 | 0.994 | 93.8 | 96.2 |
| ubfc | ablate-win15 | POS | 1965.00 | 0.66 | 2.23 | 0.71 | 0.992 | 96.8 | 97.3 |
| ubfc | methods | PRISM | 2155.00 | 0.66 | 1.95 | 0.71 | 0.994 | 98.2 | 98.7 |
| ubfc | methods | POS | 2155.00 | 0.67 | 2.19 | 0.71 | 0.993 | 98.0 | 98.7 |
| ubfc | engine | POS | 2155.00 | 0.67 | 2.19 | 0.71 | 0.993 | 98.0 | 98.7 |
| ubfc | engine-lite | POS | 2155.00 | 0.67 | 2.19 | 0.71 | 0.993 | 98.0 | 98.7 |
| ubfc | classical-only | POS | 2155.00 | 0.67 | 2.19 | 0.71 | 0.993 | 98.0 | 98.7 |
| ubfc | ablate-no-track | POS | 2155.00 | 0.67 | 2.19 | 0.71 | 0.993 | 98.0 | 98.7 |
| ubfc | ablate-motion-comp-on | POS | 2155.00 | 0.67 | 2.19 | 0.71 | 0.993 | 98.0 | 98.7 |
| ubfc | ablate-no-pose-prior | POS | 2155.00 | 0.67 | 2.19 | 0.71 | 0.993 | 98.0 | 98.7 |
| ubfc | ablate-equal-prior | POS | 2155.00 | 0.67 | 2.19 | 0.71 | 0.993 | 98.0 | 98.7 |
| ubfc | ablate-win15 | ME-rPPG | 1965.00 | 0.67 | 2.00 | 0.74 | 0.994 | 96.4 | 97.3 |
| ubfc | ablate-win8 | FacePhys | 2231.00 | 0.72 | 2.01 | 0.76 | 0.994 | 98.4 | 99.0 |
| ubfc | methods | ME-rPPG | 2155.00 | 0.74 | 1.88 | 0.80 | 0.995 | 98.1 | 98.5 |
| ubfc | engine | ME-rPPG | 2155.00 | 0.74 | 1.88 | 0.80 | 0.995 | 98.1 | 98.5 |
| ubfc | ablate-no-track | ME-rPPG | 2155.00 | 0.74 | 1.88 | 0.80 | 0.995 | 98.1 | 98.5 |
| ubfc | ablate-motion-comp-on | ME-rPPG | 2155.00 | 0.74 | 1.88 | 0.80 | 0.995 | 98.1 | 98.5 |
| ubfc | ablate-no-pose-prior | ME-rPPG | 2155.00 | 0.74 | 1.88 | 0.80 | 0.995 | 98.1 | 98.5 |
| ubfc | ablate-equal-prior | ME-rPPG | 2155.00 | 0.74 | 1.88 | 0.80 | 0.995 | 98.1 | 98.5 |
| ubfc | ablate-win8 | POS-MR | 2231.00 | 0.75 | 2.42 | 0.80 | 0.991 | 98.3 | 98.8 |
| ubfc | methods | CHROM | 2155.00 | 0.79 | 3.16 | 0.84 | 0.985 | 97.7 | 98.2 |
| ubfc | ablate-win8 | ME-rPPG | 2231.00 | 0.85 | 1.85 | 0.91 | 0.995 | 97.7 | 98.7 |
| ubfc | ablate-win8 | POS | 2231.00 | 0.88 | 3.31 | 0.92 | 0.984 | 98.0 | 98.8 |
| ubfc | ablate-win30 | ME-rPPG | 1395.00 | 0.90 | 2.64 | 0.98 | 0.990 | 91.8 | 94.9 |
| ubfc | methods | OMIT | 2155.00 | 1.11 | 4.76 | 1.19 | 0.968 | 97.0 | 97.6 |
| ubfc | methods | LGI | 2155.00 | 1.11 | 4.77 | 1.19 | 0.968 | 97.0 | 97.5 |
| ubfc | methods | 2SR | 2155.00 | 1.78 | 7.38 | 1.86 | 0.927 | 95.5 | 96.0 |
| ubfc | methods | PBV | 2155.00 | 1.84 | 6.92 | 1.86 | 0.933 | 93.8 | 95.5 |
| ubfc | methods | GREEN | 2155.00 | 3.27 | 11.53 | 3.16 | 0.827 | 91.6 | 92.6 |
| ubfc | methods | ICA | 2155.00 | 4.92 | 13.78 | 4.96 | 0.784 | 75.9 | 82.9 |
| ubfc | methods | PCA | 2155.00 | 5.81 | 15.02 | 5.80 | 0.746 | 72.0 | 79.4 |

## Protocol: stream

| dataset | config | candidate | n | MAE | RMSE | MAPE | r | acc3 | acc5 |
|---|---|---|---|---|---|---|---|---|---|
| mcd | engine | FUSED | 7253.00 | 4.99 | 10.19 | 5.65 | 0.722 | 71.7 | 76.9 |
| mcd | engine-lite | FUSED | 7253.00 | 6.34 | 12.44 | 7.15 | 0.630 | 65.8 | 72.0 |
| mcd | classical-only | FUSED | 7253.00 | 12.96 | 20.09 | 14.18 | 0.217 | 46.8 | 52.0 |
| ubfc | engine-lite | FUSED | 2123.00 | 0.61 | 1.92 | 0.65 | 0.994 | 98.2 | 98.4 |
| ubfc | engine | FUSED | 2123.00 | 0.62 | 1.89 | 0.66 | 0.995 | 98.1 | 98.5 |
| ubfc | classical-only | FUSED | 2123.00 | 0.62 | 1.76 | 0.67 | 0.995 | 97.9 | 98.6 |

### SQI gating (mcd, classical-only, stream)

| sqi>= | coverage% | MAE | RMSE | acc5 |
|---|---|---|---|---|
| 0.00 | 100.00 | 12.96 | 20.09 | 52.0 |
| 0.20 | 82.31 | 11.67 | 18.91 | 57.1 |
| 0.30 | 73.86 | 11.05 | 18.40 | 59.7 |
| 0.40 | 62.98 | 10.18 | 17.68 | 63.6 |
| 0.50 | 51.47 | 8.85 | 16.41 | 68.8 |
| 0.60 | 39.65 | 7.02 | 14.49 | 76.3 |
| 0.70 | 28.44 | 5.16 | 12.31 | 83.6 |

### SQI gating (mcd, engine, stream)

| sqi>= | coverage% | MAE | RMSE | acc5 |
|---|---|---|---|---|
| 0.00 | 100.00 | 4.99 | 10.19 | 76.9 |
| 0.20 | 84.23 | 3.75 | 8.41 | 83.1 |
| 0.30 | 77.89 | 3.29 | 7.73 | 85.6 |
| 0.40 | 69.65 | 2.76 | 6.78 | 88.2 |
| 0.50 | 61.34 | 2.24 | 5.80 | 91.2 |
| 0.60 | 52.79 | 1.76 | 4.90 | 93.9 |
| 0.70 | 43.83 | 1.36 | 4.02 | 96.2 |

### SQI gating (mcd, engine-lite, stream)

| sqi>= | coverage% | MAE | RMSE | acc5 |
|---|---|---|---|---|
| 0.00 | 100.00 | 6.34 | 12.44 | 72.0 |
| 0.20 | 83.28 | 4.58 | 9.85 | 79.4 |
| 0.30 | 76.09 | 3.99 | 8.95 | 82.2 |
| 0.40 | 66.97 | 3.33 | 7.88 | 85.7 |
| 0.50 | 58.40 | 2.71 | 6.92 | 89.0 |
| 0.60 | 49.66 | 2.14 | 5.97 | 92.4 |
| 0.70 | 40.18 | 1.52 | 4.52 | 95.6 |

### SQI gating (ubfc, classical-only, stream)

| sqi>= | coverage% | MAE | RMSE | acc5 |
|---|---|---|---|---|
| 0.00 | 100.00 | 0.62 | 1.76 | 98.6 |
| 0.20 | 99.95 | 0.62 | 1.76 | 98.6 |
| 0.30 | 99.72 | 0.60 | 1.68 | 98.8 |
| 0.40 | 99.34 | 0.60 | 1.68 | 98.8 |
| 0.50 | 98.40 | 0.58 | 1.62 | 98.9 |
| 0.60 | 96.94 | 0.55 | 1.53 | 99.1 |
| 0.70 | 93.55 | 0.48 | 1.11 | 99.4 |

### SQI gating (ubfc, engine, stream)

| sqi>= | coverage% | MAE | RMSE | acc5 |
|---|---|---|---|---|
| 0.00 | 100.00 | 0.62 | 1.89 | 98.5 |
| 0.20 | 100.00 | 0.62 | 1.89 | 98.5 |
| 0.30 | 99.91 | 0.61 | 1.84 | 98.6 |
| 0.40 | 99.53 | 0.59 | 1.74 | 98.7 |
| 0.50 | 98.87 | 0.56 | 1.54 | 98.9 |
| 0.60 | 98.21 | 0.53 | 1.43 | 99.1 |
| 0.70 | 96.18 | 0.50 | 1.30 | 99.3 |

### SQI gating (ubfc, engine-lite, stream)

| sqi>= | coverage% | MAE | RMSE | acc5 |
|---|---|---|---|---|
| 0.00 | 100.00 | 0.61 | 1.92 | 98.4 |
| 0.20 | 99.95 | 0.60 | 1.90 | 98.4 |
| 0.30 | 99.91 | 0.60 | 1.90 | 98.4 |
| 0.40 | 99.67 | 0.59 | 1.84 | 98.5 |
| 0.50 | 98.92 | 0.56 | 1.69 | 98.7 |
| 0.60 | 97.93 | 0.51 | 1.46 | 99.0 |
| 0.70 | 96.18 | 0.47 | 1.28 | 99.3 |
