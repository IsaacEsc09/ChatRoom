#include "Cliente.h"
#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>

using namespace std;

Cliente::Cliente(const std::string& host, int puerto) : host_(host), puerto_(puerto), clientSocket_(-1) {}

Cliente::~Cliente() {
    if (clientSocket_ != -1) {
        close(clientSocket_);
        if (hiloRecibir_.joinable()) {
            hiloRecibir_.join();
        }
    }
}

void Cliente::conectar() {
    clientSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket_ == -1) {
        cerr << "Error al crear el socket del cliente" << endl;
        exit(-1);
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(puerto_);
    inet_pton(AF_INET, host_.c_str(), &serverAddress.sin_addr);

    if (connect(clientSocket_, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        cerr << "Error al conectar con el servidor. Asegúrate de que el servidor está encendido." << endl;
        close(clientSocket_);
        exit(-1);
    }
}

void Cliente::enviarMensaje(const std::string& mensaje) {
    send(clientSocket_, mensaje.c_str(), mensaje.length(), 0);
}

void Cliente::recibirMensajes() {
    char buffer[512] = {0};

    while (true) {
        ssize_t bytesRecibidos = recv(clientSocket_, buffer, sizeof(buffer), 0);
        if (bytesRecibidos <= 0) {
            lock_guard<mutex> lock(coutMutex_);
            cout << "Desconectado del servidor." << endl;
            break;
        }

        lock_guard<mutex> lock(coutMutex_);
        cout << string(buffer, bytesRecibidos) << endl;
        memset(buffer, 0, sizeof(buffer));
    }
}

