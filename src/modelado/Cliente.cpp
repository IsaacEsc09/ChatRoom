#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "json.hpp" 

using namespace std;
using json = nlohmann::json;

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

    // Crear el mensaje de identificación en formato JSON
    json mensajeIdentificacion = {
        {"type", "IDENTIFY"},
        {"username", nombreCliente}
    };

    // Convertir el mensaje JSON a string y enviarlo al servidor
    string mensajeStr = mensajeIdentificacion.dump();
    send(clientSocket, mensajeStr.c_str(), mensajeStr.length(), 0);

    // Recibir la respuesta del servidor
    char buffer[1024] = {0};
    ssize_t bytesRecibidos = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRecibidos <= 0) {
        cerr << "Error al recibir respuesta del servidor." << endl;
        close(clientSocket);
        return -1;
    }

    // Procesar la respuesta JSON del servidor
    string respuesta(buffer, bytesRecibidos);
    json respuestaJson;
    try {
        respuestaJson = json::parse(respuesta);
    } catch (const json::parse_error& e) {
        cerr << "Error al parsear la respuesta JSON: " << e.what() << endl;
        close(clientSocket);
        return -1;
    }

    // Verificar el resultado de la identificación
    if (respuestaJson.contains("result") && respuestaJson["result"] == "SUCCESS") {
        cout << "Nombre/ID aceptado. Puedes comenzar a enviar mensajes." << endl;
    } else if (respuestaJson["result"] == "USER_ALREADY_EXISTS") {
        cerr << "Nombre/ID ya en uso. Intenta con otro nombre." << endl;
        close(clientSocket);
        return -1;
    } else {
        cerr << "Error desconocido: " << respuestaJson.dump() << endl;
        close(clientSocket);
        return -1;
    }

    // Bucle para enviar mensajes
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
