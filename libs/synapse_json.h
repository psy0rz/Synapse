
#ifndef CSYNAPSE_JSON_H
#define CSYNAPSE_JSON_H

bool json2Cmsg(asio::streambuf &readBuffer, Cmsg & msg);
void Cmsg2json(Cmsg &msg, string & jsonStr);

#endif
