/*
Copyright (c) 2014 Mek Entertainment, Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "mredis.h"
#include "SDL_net.h"

typedef struct mredis_s {
	TCPsocket tcpsock;
}mredis_t;

mredis mredis_init(const char* ip, int port, const char* auth) {
	mredis temp = NULL;
	IPaddress ipaddr;

	char *msg = (char*)calloc(128, sizeof(char));
	char *recv = NULL;
	bool ok = false;

	SDLNet_Init();

	SDLNet_ResolveHost(&ipaddr, ip, port);

	temp = (mredis)malloc(sizeof(mredis_t));
	temp->tcpsock = SDLNet_TCP_Open(&ipaddr);

	if (temp->tcpsock) {
		ok = true;

		if (auth != NULL) {
			sprintf (msg, "*2\r\n$4\r\nAUTH\r\n$%u\r\n%s\r\n", (unsigned)strlen(auth), auth);
			if (!mredis_sendMessage(temp, msg, strlen(msg))) {
				ok = false;
			}
		}

		sprintf(msg, "*2\r\n$4\r\nINFO\r\n$6\r\nSERVER\r\n");
		mredis_sendMessage(temp, msg, strlen(msg));

		if ((auth != NULL) && (ok) && (mredis_recieveReply(temp, &recv) == MR_STR) && (!strcmp(recv, "OK"))) {
			free(recv);
			ok = true;
		} else if(auth != NULL) {
			printf("MREDIS ERR: %s\n", recv);
			free(recv);
			ok = false;
		}

		if (mredis_recieveReply(temp, &recv) == MR_BIN) {
			int major, minor, patch;

			sscanf(strstr(recv, "redis_version:"), "redis_version:%d.%d.%d\r\n", &major, &minor, &patch);

			printf("REDIS Version: %d.%d.%d\n", major,minor,patch);

			free(recv);

			if (((major * 10000) + (minor * 100) + patch) < (20804)) {
				ok = false;
			} else {
				ok = true;
			}
		} else {
			printf("MREDIS ERR: %s\n", recv);
			free(recv);
			ok = false;
		}
	}

	if (!ok) {
		free(temp);
		temp = NULL;
	}

	free(msg);

	return temp;
}

void mredis_close(mredis rd) {
	SDLNet_TCP_Close( rd->tcpsock );
	free(rd);
}

bool mredis_sendMessage(mredis rd, const char* msg, size_t sz) {
	size_t sent = 0;
	size_t temp = 0;

	while ((sz > 0) && ((temp = SDLNet_TCP_Send(rd->tcpsock, msg + sent, sz)) > 0)) {
		sz -= temp;
		sent += temp;
	}

	if(sz > 0) {
		printf( "MREDIS ERR: %s\n", SDLNet_GetError() );
		return false;
	}

	return true;
}

mredis_return_t mredis_recieveReply(mredis rd, char** ret) {
	size_t sz = 2, sz2 = 0;
	char *recv = (char*)calloc(sz, sizeof(char));
	char *temp = *ret = (char*)calloc(sz, sizeof(char));

	mredis_return_t type = MR_ERR;

	if (SDLNet_TCP_Recv(rd->tcpsock, recv, 1) == 1) {

		switch(recv[0]) {
			case '-':
				while ((SDLNet_TCP_Recv(rd->tcpsock, recv, 1) == 1) && (recv[0] != '\r')) {
					strcat(temp, recv);

					temp = realloc(*ret, (++sz) * sizeof(char));
					*ret = temp;
				}
				SDLNet_TCP_Recv(rd->tcpsock, recv, 1);
				type = MR_ERR;
				*ret = temp;
			break;

			case '+':
			case ':':
				while ((SDLNet_TCP_Recv(rd->tcpsock, recv, 1) == 1) && (recv[0] != '\r')) {
					strcat(temp, recv);

					temp = realloc(*ret, (++sz) * sizeof(char));
					*ret = temp;
				}
				SDLNet_TCP_Recv(rd->tcpsock, recv, 1);
				type = MR_STR;
				*ret = temp;
			break;

			case '$':;
				while ((SDLNet_TCP_Recv(rd->tcpsock, recv, 1) == 1) && (recv[0] != '\r')) {
					strcat(temp, recv);

					temp = realloc(*ret, (++sz) * sizeof(char));
					*ret = temp;

					memset(recv, 0, 2 * sizeof(char));
				}
				SDLNet_TCP_Recv(rd->tcpsock, recv, 1);
				
				if(temp[0] == '-') {
					free(temp);
					temp = *ret = NULL;
					type = MR_BIN;
				} else {
					sz2 = strtol(temp, NULL, 10);
					temp = realloc(recv, (sz2 + 1) * sizeof(char));
					recv = temp;

					temp = realloc(*ret, (sz2 + 1) * sizeof(char));
					*ret = temp;

					memset(temp, 0, (sz2 + 1) * sizeof(char));
					memset(recv, 0, (sz2 + 1) * sizeof(char));

					while (sz2 > 0) {
						sz = SDLNet_TCP_Recv(rd->tcpsock, recv, sz2);
						memcpy(temp, recv, sz);
						sz2 -= sz;
						temp = temp + sz;
						memset(recv, 0, sz * sizeof(char));
					}
					SDLNet_TCP_Recv(rd->tcpsock, recv, 2);
					type = MR_BIN;
				}
			break;

			case '*':
				while ((SDLNet_TCP_Recv(rd->tcpsock, recv, 1) == 1) && (recv[0] != '\r')) {
					strcat(temp, recv);

					temp = realloc(*ret, (++sz) * sizeof(char));
					*ret = temp;
				}
				SDLNet_TCP_Recv(rd->tcpsock, recv, 1);
				type = MR_ARR;
				*ret = temp;
			break;

			default:
				free(recv);
				free(temp);

				type = MR_ERR;
				*ret = NULL;
			break;
		};
	}

	free(recv);

	return type;
}

size_t mredis_recieveArray(mredis rd, char ***data)
{
	char *buffer = NULL;
	char **temp = NULL;

	size_t sz = 0, i = 0, j = 0;

	if(mredis_recieveReply(rd, &buffer) != MR_ARR) {
		printf("MREDIS ERR: %s", buffer);
		
		free(buffer);
		*data = NULL;
		return 0;
	}

	sz = strtol(buffer, NULL, 10);
	temp = (char**)calloc(sz, sizeof(char*));

	free(buffer);

	for(i = 0; i < sz; ++i)
	{
		if(mredis_recieveReply(rd, &buffer) != MR_BIN) {
			printf("MREDIS ERR: %s", buffer);
			
			for(j = 0; j < i; ++j) {
				free(temp[i]);
			}

			free(temp);
			free(buffer);
			*data = NULL;
			return 0;
		}

		temp[i] = buffer;
	}

	*data = temp;

	return sz;
}

bool mredis_flushdb(mredis rd) {
	static const char *msg = "*1\r\n$7\r\nFLUSHDB\r\n";
	char *recv = NULL;
	
	mredis_sendMessage(rd, msg, strlen(msg));
	
	if ((mredis_recieveReply(rd, &recv) == MR_STR) && (!strcmp(recv, "OK")))
	{
		free(recv);
		return true;
	}
	
	printf("MREDIS ERR: %s", recv);
	
	free(recv);
	return false;
}

bool mredis_select(mredis rd, uint8_t i) {
	char *msg = NULL;

	size_t szIndex = (i == 0 ? 1 : (size_t)(log10(i)+1));
	size_t sz = 22 + szIndex + (szIndex == 0 ? 1 : (size_t)(log10(szIndex)+1));

	msg = (char*)calloc(sz, sizeof(char));

	sprintf(msg, "*2\r\n$6\r\nSELECT\r\n$%u\r\n%u\r\n", (unsigned)szIndex, i);
	mredis_sendMessage(rd, msg, strlen(msg));
	free(msg);
	
	if ((mredis_recieveReply(rd, &msg) == MR_STR) && (!strcmp(msg, "OK")))
	{
		free(msg);
		return true;
	}

	free(msg);
	return false;
}

void mredis_sendSORT(mredis rd, const char *key, size_t count, const char **cmd) {
	char *buffer = (char*)malloc(32 * sizeof(char));
	char *msg = NULL;

	size_t szKey = strlen(key);
	size_t sz = 19 + szKey + ((size_t)(log10(szKey)+1)) + ((size_t)(log10(count + 2)+1));
	size_t i = 0, sz2;

	for (i = 0; i < count; ++i) {
		memset(buffer, 0, 32 * sizeof(char));
		sz2 = strlen(cmd[i]);
		sprintf(buffer, "$%u", (unsigned)sz2);
		sz += strlen(buffer) + sz2 + 4;
	}

	msg = (char*)malloc(sz * sizeof(char));
	sprintf(msg, "*%u\r\n$4\r\nSORT\r\n$%u\r\n%s\r\n", (unsigned)count + 2, (unsigned)szKey, key);

	for (i = 0; i < count; ++i) {
		memset(buffer, 0, 32 * sizeof(char));
		sz = strlen(cmd[i]);
		sprintf(buffer, "$%u", (unsigned)sz);

		strcat(msg, buffer);
		strcat(msg, "\r\n");
		strcat(msg, cmd[i]);
		strcat(msg, "\r\n");
	}

	mredis_sendMessage(rd, msg, strlen(msg));
	free(buffer);
	free(msg);
}

bool mredis_SET(mredis rd, const char *key, const char *value) {
	size_t szKey = strlen(key);
	size_t szValue = strlen(value);
	
	char *msg = (char*)calloc(24 + szKey + ((size_t)(log10(szKey)+1)) + szValue + ((size_t)(log10(szValue)+1)), sizeof(char));
	sprintf(msg, "*3\r\n$3\r\nSET\r\n$%u\r\n%s\r\n$%u\r\n%s\r\n", (unsigned)szKey, key, (unsigned)szValue, value);
	
	mredis_sendMessage(rd, msg, strlen(msg));
	free(msg);
	
	if ((mredis_recieveReply(rd, &msg) == MR_STR))
	{
		free(msg);
		return true;
	}

	free(msg);
	return false;
}

void mredis_sendGET(mredis rd, const char *key) {
	size_t szKey = strlen(key);
	
	char *msg = (char*)calloc(19 + szKey + ((size_t)(log10(szKey)+1)), sizeof(char));
	sprintf(msg, "*2\r\n$3\r\nGET\r\n$%u\r\n%s\r\n", (unsigned)szKey, key);
	
	mredis_sendMessage(rd, msg, strlen(msg));
	free(msg);
}

bool mredis_HSET(mredis rd, const char *key, const char *field, const char *value) {
	if(!key || !field || !value)
		return true;
	
	size_t szKey = strlen(key);
	size_t szField = strlen(field);
	size_t szValue = strlen(value);
	
	if(!szKey || !szField || !szValue)
		return true;
	
	char *msg = (char*)calloc(30 + szKey + ((size_t)(log10(szKey)+1)) + szField + ((size_t)(log10(szField)+1)) + szValue + ((size_t)(log10(szValue)+1)) , sizeof(char));
	sprintf(msg, "*4\r\n$4\r\nHSET\r\n$%u\r\n%s\r\n$%u\r\n%s\r\n$%u\r\n%s\r\n", (unsigned)szKey, key, (unsigned)szField, field, (unsigned)szValue, value);
	
	mredis_sendMessage(rd, msg, strlen(msg));
	free(msg);
	
	if ((mredis_recieveReply(rd, &msg) == MR_STR))
	{
		free(msg);
		return true;
	}

	free(msg);
	return false;
}

void mredis_sendHMGET(mredis rd, const char *key, size_t count, const char **fields) {
	char *buffer = (char*)malloc(32 * sizeof(char));
	char *msg = NULL;

	size_t szKey = strlen(key);
	size_t sz = 20 + szKey + ((size_t)(log10(szKey)+1)) + ((size_t)(log10(count + 2)+1));
	size_t i = 0, sz2;

	for (i = 0; i < count; ++i) {
		memset(buffer, 0, 32 * sizeof(char));
		sz2 = strlen(fields[i]);
		sprintf(buffer, "$%u", (unsigned)sz2);
		sz += strlen(buffer) + sz2 + 4;
	}

	msg = (char*)malloc(sz * sizeof(char));
	sprintf(msg, "*%u\r\n$5\r\nHMGET\r\n$%u\r\n%s\r\n", (unsigned)count + 2, (unsigned)szKey, key);

	for (i = 0; i < count; ++i) {
		memset(buffer, 0, 32 * sizeof(char));
		sz = strlen(fields[i]);
		sprintf(buffer, "$%u", (unsigned)sz);

		strcat(msg, buffer);
		strcat(msg, "\r\n");
		strcat(msg, fields[i]);
		strcat(msg, "\r\n");
	}

	mredis_sendMessage(rd, msg, strlen(msg));
	free(buffer);
	free(msg);
}

bool mredis_ZADD(mredis rd, const char *key, uint32_t score, const char *member) {
	if(!key || !member)
		return true;
	
	size_t szKey = strlen(key);
	size_t szScore = score != 0 ? ((size_t)(log10(score)+1)) : 1;
	size_t szMember = strlen(member);
	
	if(!szKey || !szMember)
		return true;
	
	char *msg = (char*)calloc(30 + szKey + ((size_t)(log10(szKey)+1)) + szScore + ((size_t)(log10(szScore)+1)) + szMember + ((size_t)(log10(szMember)+1)), sizeof(char));
	sprintf(msg, "*4\r\n$4\r\nZADD\r\n$%u\r\n%s\r\n$%u\r\n%u\r\n$%u\r\n%s\r\n", (unsigned)szKey, key, (unsigned)szScore, score, (unsigned)szMember, member);
	
	mredis_sendMessage(rd, msg, strlen(msg));
	free(msg);
	
	if ((mredis_recieveReply(rd, &msg) == MR_STR))
	{
		free(msg);
		return true;
	}

	free(msg);
	return false;
}

size_t mredis_ZCARD(mredis rd, const char *key){
	size_t szKey = strlen(key);
	size_t sz = 21 + szKey + ((size_t)(log10(szKey)+1));

	char *msg = (char*)calloc(sz, sizeof(char));

	sprintf(msg, "*2\r\n$5\r\nZCARD\r\n$%u\r\n%s\r\n", (unsigned)szKey, key);

	mredis_sendMessage(rd, msg, strlen(msg));
	free(msg);
	
	if (mredis_recieveReply(rd, &msg) == MR_STR) {
		sz = strtol(msg, NULL, 10);
		free(msg);
		return sz;
	}
	
	printf("MREDIS ERR: %s", msg);
	free(msg);
	return 0;
}

void mredis_sendZRANGE(mredis rd, const char *key, int start, int stop, bool withScores) {
	char *msg = NULL;

	size_t szKey = strlen(key);
	size_t szStart = start != 0 ? ((size_t)(log10(fabs(start))+1) + (start < 0 ? 1 : 0)) : 1;
	size_t szStop = stop != 0 ?((size_t)(log10(fabs(stop))+1) + (stop < 0 ? 1 : 0)) : 1;
	size_t sz = 0;

	if (withScores) {
		sz = 49 + ((size_t)(log10(szKey)+1)) + szKey + szStart + ((size_t)(log10(szStart)+1)) + szStop + ((size_t)(log10(szStop)+1));
		msg = (char*)calloc(sz, sizeof(char));
		sprintf(msg, "*5\r\n$6\r\nZRANGE\r\n$%u\r\n%s\r\n$%u\r\n%d\r\n$%u\r\n%d\r\n$10\r\nWITHSCORES\r\n",
			(unsigned)szKey, key, (unsigned)szStart, start, (unsigned)szStop, stop);
	} else {
		sz = 32 + ((size_t)(log10(szKey)+1)) + szKey + szStart + ((size_t)(log10(szStart)+1)) + szStop + ((size_t)(log10(szStop)+1));
		msg = (char*)calloc(sz, sizeof(char));
		sprintf(msg, "*5\r\n$6\r\nZRANGE\r\n$%u\r\n%s\r\n$%u\r\n%d\r\n$%u\r\n%d\r\n",
			(unsigned)szKey, key, (unsigned)szStart, start, (unsigned)szStop, stop);
	}

	mredis_sendMessage(rd, msg, strlen(msg));

	free(msg);
}
