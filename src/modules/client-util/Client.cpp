/*
	Copyright 2010, Sumeet Chhetri 

    Licensed under the Apache License, Version 2.0 (the "License"); 
    you may not use this file except in compliance with the License. 
    You may obtain a copy of the License at 

        http://www.apache.org/licenses/LICENSE-2.0 

    Unless required by applicable law or agreed to in writing, software 
    distributed under the License is distributed on an "AS IS" BASIS, 
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
    See the License for the specific language governing permissions and 
    limitations under the License.  
 */
/*
 * Client.cpp
 *
 *  Created on: Mar 27, 2010
 *      Author: sumeet
 */

#include "Client.h"

Client::Client() {
	logger = LoggerFactory::getLogger("Client");
	connected = false;
}

Client::~Client() {
	closeConnection();
}

bool Client::connection(const std::string& host, const int& port)
{
	if(host=="localhost")
	{
		return connectionUnresolv(host, port);
	}

	struct sockaddr_in *remote;
	int tmpres;
	char *ip;

	sockfd = create_tcp_socket();
	ip = get_ip((char*)host.c_str());
	fprintf(stderr, "IP is %s\n", ip);
	remote = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in *));
	remote->sin_family = AF_INET;
	tmpres = inet_pton(AF_INET, ip, (void *)(&(remote->sin_addr.s_addr)));
	if( tmpres < 0)
	{
		perror("Can't set remote->sin_addr.s_addr");
		return false;
	}
	else if(tmpres == 0)
	{
		fprintf(stderr, "%s is not a valid IP address\n", ip);
		return false;
	}
	remote->sin_port = htons(port);

	if(connect(sockfd, (struct sockaddr *)remote, sizeof(struct sockaddr)) < 0){
		perror("Could not connect");
		connected = false;
	} else {
		connected = true;
	}
	free(remote);
	free(ip);
	
	return connected;
}


bool Client::connectionUnresolv(const std::string& host, const int& port)
{
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    memset(s, 0, INET6_ADDRSTRLEN);
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    std::string sport = CastUtil::lexical_cast<std::string>(port);
    if ((rv = getaddrinfo(host.c_str(), sport.c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return false;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
        	closesocket(sockfd);
            perror("client: connect");
            connected = false;
            continue;
        } else {
        	connected = true;
        }
        break;
    }

    if (p == NULL) {
    	connected = false;
        fprintf(stderr, "client: failed to connect\n");
        return false;
    }

    //inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
    //        s, sizeof s);
    //printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    return connected;
}

void Client::setSocketBlocking()
{
	#ifdef OS_MINGW
		u_long bMode = 0;
		ioctlsocket(sockfd, FIONBIO, &bMode);
	#else
		fcntl(sockfd, F_SETFL, O_SYNC);
	#endif
}

void Client::setSocketNonBlocking()
{
	#ifdef OS_MINGW
		u_long iMode = 1;
		ioctlsocket(sockfd, FIONBIO, &iMode);
	#else
		fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK);
	#endif
}

int Client::sendData(std::string data)
{
	int sent = 0;
	while(data.length()>0)
	{
		int tmpres = send(sockfd, data.c_str(), data.length(), 0);
		if(tmpres == -1){
			perror("Can't send data");
		}
		data = data.substr(tmpres);
	}
	return sent;
}

std::string Client::getTextData(const std::string& hdrdelm, const std::string& cntlnhdr)
{
	int er=-1;
	bool flag = true;
	std::string alldat;
	int cntlen;
	char buf[MAXBUFLE];
	memset(buf, 0, MAXBUFLE);
	BIO* sbio=BIO_new_socket(sockfd,BIO_NOCLOSE);
	BIO* io=BIO_new(BIO_f_buffer());
	BIO_push(io,sbio);
	bool isTE = false;
	std::string tehdr = "transfer-encoding";
	while(flag)
	{
		er = BIO_gets(io,buf,MAXBUFLE-1);
		if(er==0)
		{
			if(io!=NULL)BIO_free_all(io);
			//logger << "\nsocket closed before being serviced" <<std::flush;
			return alldat;
		}
		if(!strcmp(buf,hdrdelm.c_str()))
		{
			std::string tt(buf, er);
			alldat += tt;
			break;
		}
		std::string temp(buf, er);
		alldat += temp;
		std::string ltemp = StringUtil::toLowerCopy(temp);
		if(ltemp.find(cntlnhdr)!=std::string::npos)
		{
			std::string cntle = temp.substr(temp.find(": ")+2);
			StringUtil::trim(cntle);
			try
			{
				cntlen = CastUtil::lexical_cast<int>(cntle);
			}
			catch(...)
			{
				logger << "bad lexical cast" <<std::endl;
			}
		}
		else if(ltemp.find(tehdr)!=std::string::npos)
		{
			std::string cntle = ltemp.substr(ltemp.find(": ")+2);
			StringUtil::trim(cntle);
			if(cntle=="chunked") {
				isTE = true;
			}
		}
		memset(&buf[0], 0, sizeof(buf));
	}
	memset(&buf[0], 0, sizeof(buf));
	while(isTE)
	{
		er = BIO_gets(io,buf,MAXBUFLE-1);
		if(er==0)
		{
			if(io!=NULL)BIO_free_all(io);
			//logger << "\nsocket closed before being serviced" <<std::flush;
			return alldat;
		}
		std::string bytesstr(buf, er);
		StringUtil::replaceFirst(bytesstr,"\r\n","");
		int bytesToRead = (int)StringUtil::fromHEX(bytesstr);
		if(bytesToRead==0)
		{
			er = BIO_read(io,buf,2);
			if(io!=NULL)BIO_free_all(io);
			//logger << "\nsocket closed before being serviced" <<std::flush;
			return alldat;
		}
		memset(&buf[0], 0, sizeof(buf));
		er = BIO_read(io,buf,bytesToRead);
		if(er==0)
		{
			if(io!=NULL)BIO_free_all(io);
			//logger << "\nsocket closed before being serviced" <<std::flush;
			return alldat;
		}
		std::string temp(buf, er);
		alldat += temp;
		er = BIO_read(io,buf,2);
		if(er==0)
		{
			if(io!=NULL)BIO_free_all(io);
			//logger << "\nsocket closed before being serviced" <<std::flush;
			return alldat;
		}
	}
	while(cntlen>0)
	{
		//logger << "reading conetnt " << cntlen;
		int toRead = cntlen;
		if(cntlen>MAXBUFLE)
			toRead = MAXBUFLE - 1;
		er = BIO_read(io,buf,toRead);
		if(er==0)
		{
			if(io!=NULL)BIO_free_all(io);
			//logger << "\nsocket closed before being serviced" <<std::flush;
			return alldat;
		}
		std::string temp(buf, er);
		alldat += temp;
		cntlen -= er;
		memset(&buf[0], 0, sizeof(buf));
	}
	return alldat;
}

int Client::receive(std::string& buf, const int& flag)
{
	char buff[MAXBUFLE+1];
	memset(buff, 0, sizeof(buff));
	int t = recv(sockfd, buff, MAXBUFLE, flag);
	buf = buff;
	memset(buff, 0, sizeof(buff));
	return t;
}

int Client::receivelen(std::string& buf, const int& len, const int& flag)
{
	char buff[len+1];
	memset(buff, 0, sizeof(buff));
	int t = recv(sockfd, buff, len, flag);
	buf = buff;
	memset(buff, 0, sizeof(buff));
	return t;
}

int Client::sendlen(const std::string& buf, const int& len)
{
	return send(sockfd, buf.c_str(), len, 0);
}

std::string Client::getBinaryData(const int& len, const bool& isLengthIncluded)
{
	//logger << len;
	std::string alldat;
	char buf1[len];
	memset(buf1, 0, len);
	recv(sockfd, buf1, len, 0);
	for (int var = 0; var < len; ++var) {
		alldat.push_back(buf1[var]);
	}

	int leng = getLengthCl(alldat, len);
	if(isLengthIncluded)
	{
		leng -= len;
	}
	char buf[leng];
	memset(buf, 0, leng);
	recv(sockfd, buf, leng, 0);
	for (int var = 0; var < leng; ++var) {
		alldat.push_back(buf[var]);
	}
	return alldat;
}

void Client::closeConnection()
{
	if(!connected)return;
	connected = false;
	closesocket(sockfd);
}

bool Client::isConnected()
{
	return connected && ClientInterface::isConnected(sockfd);
}

std::string Client::getData()
{
	int numbytes;
	char buf[MAXBUFLE];
	memset(buf, 0, sizeof(buf));
	while ((numbytes = recv(sockfd, buf, MAXBUFLE-1, 0)) == -1)
	{
		//perror("recv");
		if(errno!=EAGAIN)
			return "";
		//exit(1);
	}
	if(numbytes==0)
	{
		connected = false;
		closesocket(sockfd);
		return "";
	}
	std::string data(buf,buf+numbytes);
	memset(&buf[0], 0, sizeof(buf));
	return data;
}
