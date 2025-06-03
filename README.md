# SP_Project_OctaFilp Execution Guide _(Updated 25.06.04)_
## Handle one-wire issue
Disable 1-Wire in raspi-config:
1. Open a terminal on your Pi.
```bash
sudo raspi-config
```
2. Navigate to: Interface Options → 1-Wire → Disable
3. Exit and reboot your Pi: `sudo reboot`
   
## Board Alone
### (Build board)
```bash
g++ -DBOARD_MAIN -o board board.c -I./rpi-rgb-led-matrix/include -L./rpi-rgb-led-matrix/lib -lrgbmatrix
```
### (Run board)
```bash
$ sudo ./board
```

## Client including board
### (Build Client)
```bash
g++ -o client client.c board.c -I./rpi-rgb-led-matrix/include -L./rpi-rgb-led-matrix/lib -lrgbmatrix -lcjson
```
### (Run Client)
```bash
sudo ./client -ip 10.8.128.233 -port 8080 -username Alice
```

