# SP_Project_OctaFilp

g++ -o client client.c board.c \
    -I~/rpi-rgb-led-matrix/include \
    -L~/rpi-rgb-led-matrix/lib -lrgbmatrix \
    -lcjson -lpthread
