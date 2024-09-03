#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <map>
#include <mutex>
#include <arpa/inet.h>

using namespace std;

mutex coutMutex;
mutex clientMutex;
map<string, thread> clientes; // Diccionario para manejar los hilos de los clientes

void manejarCliente(int clientSocket) {
    char buffer[1024] = {0};
    string nombreCliente;

    // Recibir el nombre del cliente
    ssize_t bytesRecibidos = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRecibidos <= 0) {
        cerr << "Error al recibir el nombre del cliente o conexión cerrada." << endl;
        close(clientSocket);
        return;
    }

    nombreCliente = string(buffer, bytesRecibidos);

    lock_guard<mutex> lock(clientMutex);
    if (clientes.find(nombreCliente) == clientes.end()) {
        // Si el nombre no está en uso, agregar al diccionario y crear un hilo
        clientes[nombreCliente] = thread([clientSocket, nombreCliente]() {
            char buffer[1024] = {0};
            while (true) {
                ssize_t bytesRecibidos = recv(clientSocket, buffer, sizeof(buffer), 0);
                if (bytesRecibidos == 0) {
                    lock_guard<mutex> lock(coutMutex);
                    cout << "Cliente " << nombreCliente << " desconectado." << endl;
                    break;
                } else if (bytesRecibidos < 0) {
                    lock_guard<mutex> lock(coutMutex);
                    cerr << "Error al recibir datos de " << nombreCliente << endl;
                    break;
                }

                lock_guard<mutex> lock(coutMutex);
                cout << nombreCliente << ": " << buffer << endl;
                memset(buffer, 0, sizeof(buffer));
            }
            close(clientSocket);
            lock_guard<mutex> lock(clientMutex);
            clientes.erase(nombreCliente);
        });
        clientes[nombreCliente].detach();
        string successMsg = "IDENTIFY_SUCCESS";
        send(clientSocket, successMsg.c_str(), successMsg.length(), 0);
    } else {
        string errorMsg = "USER_ALREADY_EXISTS";
        send(clientSocket, errorMsg.c_str(), errorMsg.length(), 0);
        close(clientSocket);
    }
}

void aceptarConexiones(int serverSocket) {
    while (true) {
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket < 0) {
            cerr << "Error al aceptar la conexión: " << strerror(errno) << endl;
            continue;
        }

        cout << "Cliente conectado. Solicitando identificación..." << endl;
        thread(manejarCliente, clientSocket).detach();
    }
}

int main() {
    //Socket del servidor
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        cerr << "Error al crear el socket del servidor: " << strerror(errno) << endl;
        return -1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(0);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        cerr << "Error al vincular el socket: " << strerror(errno) << endl;
        close(serverSocket);
        return -1;
    }

    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getsockname(serverSocket, (struct sockaddr*)&addr, &len) == -1) {
        cerr << "Error al obtener el puerto asignado: " << strerror(errno) << endl;
        close(serverSocket);
        return -1;
    }

    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ipStr, sizeof(ipStr));

    cout << "Servidor iniciado en la IP: " << ipStr << " y el puerto: " << ntohs(addr.sin_port) << endl;

    if (listen(serverSocket, 5) < 0) {
        cerr << "Error al escuchar en el socket: " << strerror(errno) << endl;
        close(serverSocket);
        return -1;
    }

    cout << "Esperando conexiones..." << endl;

    thread hiloAceptarConexiones(aceptarConexiones, serverSocket);
    hiloAceptarConexiones.join();

    close(serverSocket);

    return 0;
}
