# include "../headers/server.hpp"

Server* Server::_instance = NULL;
// =============================================================================
// CONSTRUCTORS ================================================================
// Server::Server() : _server_port("6667")
// {}

Server::Server(char **av) : _pwd(av[2]), _listener(0),
    _server_port(av[1]), _pfds(0),
    _clients(), _count_clients(0),
    _channels()
{
    time_t launch_date = std::time(0);
    _launch_date = std::asctime(std::localtime(&launch_date));
    _instance = this;
    _parse_conf_file();
    _get_listener_socket();
}


// =============================================================================
// DESTRUCTORS =================================================================
Server::~Server()
{
	_instance = NULL;
}

// =============================================================================
// MODIFIERS ===================================================================

// void
// Server::signalHandler(int signum)
// {
// 	(void)signum;
// 	ctrl_C();
// 	exit(0);
// }

// void
// Server::ctrl_C()
// {
// 	for (std::vector<struct pollfd>::iterator it = _instance->_pfds.begin(); it != _instance->_pfds.end(); it++)
//         close(it->fd);
//     _instance->_pfds.clear();
// 	_instance->_server_opers.clear();
// 	_instance->_clients.clear();
// 	_instance->_channels.clear();
// }

// =============================================================================
// METHODS =====================================================================
void
Server::_handle_data(std::vector<struct pollfd>::iterator &it)
{
    //A single message is a string of characters with a maximum length of 512 characters.
    //The end of the string is denoted by a CR-LF (Carriage Return - Line Feed) pair (i.e., “\r\n”).
    //There is no null terminator. The 512 bytes limit includes this delimiter, meaning that a message only has space for 510 useful characters.
	char    buff[512];

	memset(&buff, 0, sizeof(buff));
    // If not the listener, we're just a regular client
    int nbytes = recv(it->fd, buff, sizeof(buff) - 1, 0);
    int sender_fd = it->fd;

    if (nbytes <= 0)
    {
        if (nbytes == 0) // Got connection closed by client. Ici gestion du ctrl+C d'un client
        {
            std::cout << "pollServer: socket #" << sender_fd << " hung up" << std::endl; // Connection closed
            if (!_clients[sender_fd].getUser().empty())
                _count_clients--;
           std::cout << GREEN << "[server] " << "clients connected = " << _count_clients << RESET << std::endl;
        }
        else //Got error //TODO : verifier si on doit egalement close le fd et supprimer le client. verifer erno (EWOULDBLOCK)
        {
            perror("recv");
            return;
        }
        _clients[sender_fd].setQuitMsg("abrupt client aborting");
        _clients[sender_fd].addMsgBuffer(ERROR(_clients[sender_fd].getQuitMsg()));

        int temp = it->fd;
        _clients[sender_fd].delete_client();
        close(temp);
    }
    else
    {
        std::cout   << PURPLE << "[client] " << "fd = " << it->fd << " | "
                    << std::string(buff, 0, nbytes) << RESET << std::endl ;
		std::string ss1 = buff;
        _clients[sender_fd].setBuff(ss1);
        if (_clients[sender_fd].getBuff() == "\n")
        {
            _clients[sender_fd].clearBuff();
            return ;
        }
        if (*(_clients[sender_fd].getBuff().end() - 1) == '\n') //condition pour ctrl+D
        {
            _clients[sender_fd].parse_lines(_clients[sender_fd].getBuff());
            if (_clients[sender_fd].getSocketConnexion() == true)
                _clients[sender_fd].clearBuff();
        }
    }
}

void
Server::_add_new_client(std::vector<struct pollfd> &new_pollfds)
{
    int             reuseaddr = 1;
    struct sockaddr_storage remoteaddr; // Client address
    socklen_t               addrlen;
    int						newfd; // Newly accept()ed socket descriptor

    // If listener is ready to read, handle new connection
    addrlen = sizeof(remoteaddr);
    newfd = accept(_listener, (struct sockaddr *)&remoteaddr,&addrlen);
    if (newfd == -1)
        perror("accept");
    else
    {
        // En cas de crash, permet d'avoir un nouveau socket sur le meme port
        setsockopt(newfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int));
        struct pollfd new_poll_fd;
        new_poll_fd.fd = newfd; // the socket descriptor
        new_poll_fd.events = POLLIN | POLLOUT;
        new_pollfds.push_back(new_poll_fd);

        Client client(newfd, this);
		_clients.insert(std::pair<int,Client>(newfd, client));
    }
}

void
Server::_handle_pollout(int fd)
{
    if (!_clients[fd].getMsgBuffer().empty())
    {
        //meilleure gestion des messages faisant + de 512 caracteres : envoi en plusieurs paquets via plusieurs sends
        if (_clients[fd].getMsgBuffer().size() > 512)
        {
            std::string tmp = mess_trunc(_clients[fd].getMsgBuffer());
            std::cout << GREEN << "[server] : message sent to client " << fd << " >> " << tmp << RESET << std::endl;
            send(fd, tmp.c_str(), tmp.size(), 0);
            _clients[fd].setMsgBuffer( _clients[fd].getMsgBuffer().substr(512, std::string::npos));

        }
        else
        {
            std::cout << GREEN << "[server] : message sent to client " << fd << " >> " << _clients[fd].getMsgBuffer() << RESET << std::endl;
            send(fd, _clients[fd].getMsgBuffer().c_str(), _clients[fd].getMsgBuffer().size(), 0);
            _clients[fd].clearMsgBuffer();
        }

        if (_clients[fd].getSocketConnexion() == false)
        {
            close(fd);
            _clients[fd].delete_client();
        }
    }
}

void
Server::_poll_loop(void)
{
    struct pollfd first;

    first.fd = _listener; // the socket descriptor
    first.events = POLLIN; // Report ready to read on incoming connection
    _pfds.push_back(first);

    // Redirect standard input to the socket file descriptor
    if (dup2(_listener, STDIN_FILENO) == -1) {
        std::cout<<"Dup2 fail!\n"; // Handle error ?
        return;
    }
    bool first_loop = true;
    while (int_sign == 0)
    {
        //TODO il reste 2 still reachables avec valgrind qu on a pas reussi a virer, meme avec le signal handler avec ctrl C
		// signal(SIGINT, signalHandler);
        std::vector<pollfd> new_pollfds; // tmp struct hosting potential newly-created fds

        if (first_loop == true)
        {
		    std::cout << GREEN <<  "[server] is listening in fd = "<< _listener << RESET << std::endl;
            first_loop = false;
        }
        int poll_count = poll((pollfd *)&_pfds[0], _pfds.size(), -1);
        if (poll_count == -1)
        {
            perror("poll");
            return;
        }
        std::vector<struct pollfd>::iterator end = _pfds.end();
        for (std::vector<struct pollfd>::iterator it = _pfds.begin(); it != end; it++)
        {
            std::string input;
            std::getline(std::cin, input);

            if (it->revents & POLLIN) // Check if someone's ready to read
            {
                if (it->fd == _listener)
                    _add_new_client(new_pollfds);
                else
                    _handle_data(it);
            }
            else if (it->revents & POLLOUT) // "Alert me when I can send() data to this socket without blocking."
                _handle_pollout(it->fd);
            // POLLERR :Condition d'erreur (uniquement en sortie).
            // POLLNVAL :Requête invalide : fd nest pas ouvert (uniquement en sortie)
            // POLLHUP :Déconnexion (uniquement en sortie).
            else if (it->revents & POLLHUP || it->revents & POLLERR || it->revents & POLLNVAL)
            {
                std::cout << "Client at socket #" << it->fd << " disconnected." << std::endl;
                _clients[it->fd].socketDisconnect();
                _clients[it->fd].delete_client();
            }

        }
        _pfds.insert(_pfds.end(), new_pollfds.begin(), new_pollfds.end()); // Add the range of NEW_pollfds in poll_fds (helps recalculating poll_fds.end() in the for loop)
    }

}

void
Server::_get_listener_socket(void)
{
    int             reuseaddr = 1;
    int             rv;
    struct addrinfo hints, *ai, *temp;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, _server_port, &hints, &ai)) != 0)
    {
        std::cerr << gai_strerror(rv);
        return;
    }
    // Boucle pour trouver une adresse qui fonctionne avec l'ouverture du socket
    for(temp = ai; temp != NULL; temp = temp->ai_next)
    {
        // point d'ecoute dans le vide, pas encore connecter au Server
        _listener = socket(temp->ai_family, temp->ai_socktype, temp->ai_protocol);
        if (_listener < 0)
			continue;

        // En cas de crash, permet d'avoir un nouveau socket sur le meme port
        setsockopt(_listener, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int));

        // connexion entre le Server et lepoint d'ecoute (socket)
        if (bind(_listener, temp->ai_addr, temp->ai_addrlen) < 0)
        {
            close(_listener);
            continue;
        }
        break;
    }
    freeaddrinfo(ai);
    // fd du socket et en mode ecoute de connexions rentrantes de clients
    if (temp == NULL || listen(_listener, SOMAXCONN) == -1)
    {
        std::cerr << "error getting listening socket\n";
        return;
    }
}



void
Server::_parse_conf_file()
{
    std::string     filename = "./ircd.conf";
    std::ifstream   in_file;

    in_file.open(filename.c_str(), std::ios::in);
    if (!in_file.is_open()) {
        std::cout << "please create the conf file './ircd.conf'" << std::endl;
        return ;
    }

    std::string line;
    std::string word;
	while (std::getline(in_file, line)) {

        std::stringstream	strst(line);
        if (line.substr(0, 5) != "oper ")
            continue;

        struct server_oper newOper;
        while (std::getline(strst, word, ' '))
        {
            if (word == "oper")
                continue;
            if (newOper.name.empty())
                newOper.name = word;
            else if (newOper.pwd.empty())
                newOper.pwd = word;
            else
                newOper.host = word;
        }
        _server_opers.push_back(newOper);
	}

    if (in_file.is_open())
		in_file.close();
}
