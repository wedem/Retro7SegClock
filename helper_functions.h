
void getACK(char *buffer) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", buffer );
}
