# reliableUDP
Reliable data transfert over UDP


Per l’utilizzo del software, eseguire i programmi da 2 terminali linux, portarsi nella cartella principale del progetto “reliableUDP”, quindi:
-	COMPILARE I SORGENTI
Eseguire il comando make, il quale genererà gli eseguibili del client e del server, rispettivamente in “out/client.out” e “out/server.out”.

-	ESECUZIONE DEL SERVER
Per avviare il server, aprire un terminale ed eseguire il comando ./out/server.out
Il server rimmarrà quindi in ascolto di richieste di connessione da parte dei client. Una volta avvenuta una richiesta viene effettuato l’handshake con il client, ed il server sarà in attesa di ricevere sia una richiesta dal client connesso, sia una nuova richiesta di connessione.
Per terminare il server eseguire il comando “Ctrl + c”.

-	ESECUZIONE DEL CLIENT
Per avviare il client, aprire un secondo terminale ed eseguire il comando ./out/client.out
Il client si connetterà al server attraverso l’handshake e viene visulizzata una lista dei file già disponibili al client, e la lista dei comandi. Si hanno 20 secondi per la scelta del comando da inviare al server, altrimenti viene chiusa la connessione. 
Per terminare il client eseguire il comando “4) Close Connection”.

