#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

int main() {
    string host;
    int puerto;
    string nombreCliente;

    cout << "Ingresa la dirección del servidor: ";
    cin >> host;
    cout << "Ingresa el puerto: ";
    cin >> puerto;

    // Crear el socket del cliente
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        cerr << "Error al crear el socket del cliente" << endl;
        return -1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(puerto);
    inet_pton(AF_INET, host.c_str(), &serverAddress.sin_addr);

    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        cerr << "Error al conectar con el servidor. Asegúrate de que el servidor está encendido." << endl;
        close(clientSocket);
        return -1;
    }

    cout << "Ingresa tu nombre/ID: ";
    cin.ignore();
    getline(cin, nombreCliente);

    // Enviar el nombre/ID al servidor
    send(clientSocket, nombreCliente.c_str(), nombreCliente.length(), 0);

    char buffer[1024] = {0};
    ssize_t bytesRecibidos = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRecibidos <= 0) {
        cerr << "Error al recibir respuesta del servidor." << endl;
        close(clientSocket);
        return -1;
    }

    string respuesta(buffer);
    if (respuesta == "IDENTIFY_SUCCESS") {
        cout << "Nombre/ID aceptado. Puedes comenzar a enviar mensajes." << endl;
    } else if (respuesta == "USER_ALREADY_EXISTS") {
        cerr << "Nombre/ID ya en uso. Intenta con otro nombre." << endl;
        close(clientSocket);
        return -1;
    }

    string mensaje;
    while (true) {
        cout << "Mensaje: (o '/exit' para salir): ";
        getline(cin, mensaje);

        if (mensaje == "/exit") {
            break;
        }

        send(clientSocket, mensaje.c_str(), mensaje.length(), 0);
    }

    close(clientSocket);

    return 0;
}
