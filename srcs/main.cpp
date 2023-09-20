#include "../headers/server.hpp"
#include "../headers/client.hpp"
#include "../headers/ft_irc.hpp"

int int_sign = 0;

void signalHandler(int a)
{
    (void) a;
    int_sign = 1;
    std::cout << "int_sign = " << int_sign << std::endl;
}

int main (int ac, char **av)
{
    if (ac != 3)
    {
        std::cout << "Please run the executable as follow : /ircserv <port> <password>" << std::endl;
        return (2);
    }
    signal(SIGINT, signalHandler);

    std::string port(av[1]);
    for (size_t i = 0; i < port.size(); i++)
    {
        if (!std::isdigit(port[i]))
        {
            std::cout << "Please use a valid port number : /ircserv <port> <password>" << std::endl;
            return (2);
        }
    }
    Server serv(av);
    serv._poll_loop();
    return (0);
}

/*
nc -C 6667 123
CAP LS
PASS 123
NICK l
USER li li localhost :li
*/
