#ifndef __REQUEST_H__

void request_handle(int fd);

void request_handle_SFF(int fd, char *method, char* uri, char* version);

#endif // __REQUEST_H__
