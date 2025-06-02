# SP_Project_OctaFilp
```bash
sudo apt install lib-cjson-dev 
gcc -o client client.c -lcjson 
g++ -o board board.c -I/your/path/rpi-rgb-led-matrix/include -L/your/path/rpi-rgb-led-matrix/lib -lrgbmatrix 
ex) 
  g++ -o board board.c -I/home/dykwak/rpi-rgb-led-matrix/include -L/home/dykwak/rpi-rgb-led-matrix/lib -lrgbmatrix
./client -ip {ip} -port {port} -username {username} 
sudo ./board 
```

# Updated 25.06.02
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
$ g++ -DBOARD_MAIN -o board board.c -I/your/path/rpi-rgb-led-matrix/include -L/your/path/rpi-rgb-led-matrix/lib -lrgbmatrix
```
```bash
# Example
$ g++ -DBOARD_MAIN -o board board.c -I/home/dykwak/rpi-rgb-led-matrix/include -L/home/dykwak/rpi-rgb-led-matrix/lib -lrgbmatrix
```
### (Run board)
```bash
$ sudo ./board
```

## Client including board
### (Build Client)
```bash
g++ -o client client.c board.c -I/your/path/rpi-rgb-led-matrix/include -L/your/path/rpi-rgb-led-matrix/lib -lrgbmatrix -lcjson
```
```bash
# Example
g++ -o client client.c board.c -I/home/dykwak/rpi-rgb-led-matrix/include -L/home/dykwak/rpi-rgb-led-matrix/lib -lrgbmatrix -lcjson
```
### (Run Client)
```bash
sudo ./client -ip 10.8.128.233 -port 8080 -username Alice
```

