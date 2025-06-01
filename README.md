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
