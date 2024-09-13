# Compilacion.  
Una vez clonado el repositorio "ChatRoom", debe situarse en el directorio mismo y seguir las siguientes instrucciones para poder ejercutarlo:  
`$ mkdir build`  
`$ cd build`  
`$ cmake ..`  
`$ make`  
Se generarán dos ejecutables: "servidor" y "cliente".  
Para correr/ejecutar el servidor debe estar situado en la carpeta /build y dar el comando:  
`$ ./servidor <puerto>`  
Para el cliente es lo mismo:  
`$ ./cliente`  

#Comandos para el cliente.  
Consultar la lista de usuarios conectados al servidor, con sus respectivos estados:  
`$ /user_list`  
Cambiar el estado:  
`$ /status_ACTIVE`  
`$ /status_AWAY`  
`$ /status_BUSY`  
Desconectarse del servidor:  
`$ /exit`  
