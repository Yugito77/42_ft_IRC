# include "../headers/client.hpp"

// =============================================================================
// CONSTRUCTORS ================================================================
Client::Client()
{}

Client::Client(int client_id, Server *server) : _nick(), _user(), _user_infos(),
		_client_id_str(change_to_str(client_id)),
		_client_id(client_id),
		// _data_connexion(0),
		_buff(), _parsed_cmd(), _connected(false),
		_server(server), _socket_connected(true),
		_operator(false), _canals(),
		_quit_msg(), _is_server_oper(false),
		_modes(), _msg_buffer(), _pass()
{}


// =============================================================================
// DESTRUCTORS =================================================================
Client::~Client()
{}


// =============================================================================
// GETTERS / SETTERS ===========================================================
std::string
Client::getUser()
{
    return _user;
}

std::string
Client::getNick()
{
    return _nick;
}

std::string
Client::getBuff()
{
    return _buff;
}

void
Client::setBuff(std::string ss1)
{
	_buff = _buff + ss1;
}

void
Client::clearBuff()
{
	_buff.clear();
}

void
Client::socketDisconnect()
{
    _socket_connected = false;
}

bool
Client::getSocketConnexion()
{
    return _socket_connected;
}

void
Client::setQuitMsg(std::string msg)
{
	_quit_msg = msg;
}

std::string
Client::getQuitMsg()
{
	return _quit_msg;
}

std::string
Client::getMsgBuffer()
{
	return _msg_buffer;
}
void
Client::clearMsgBuffer()
{
	_msg_buffer.clear();
}
void
Client::addMsgBuffer(std::string msg)
{
	_msg_buffer += msg;
}
void
Client::setMsgBuffer(std::string msg)
{
	_msg_buffer = msg;
}
// =============================================================================
// METHODS =====================================================================

void
Client::search_command()
{
	if (_parsed_cmd[0] == "PASS")
		pass();
	else if (_parsed_cmd[0] == "NICK")
		nick();
	else if (_parsed_cmd[0] == "USER")
		user();
	else if (_parsed_cmd[0] == "PING")
		ping();
	else if (_parsed_cmd[0]== "JOIN")
		join();
	else if (_parsed_cmd[0]== "PRIVMSG")
		privmsg();
	else if (_parsed_cmd[0]== "MSG")
		msg();
	else if (_parsed_cmd[0] == "QUIT")
		quit(*this);
	else if (_parsed_cmd[0]== "PART")
	  	part();
	else if (_parsed_cmd[0]== "TOPIC")
	  topic();
	else if (_parsed_cmd[0]== "LIST")
	  list();
	else if (_parsed_cmd[0]== "NAMES")
	  names();
	else if (_parsed_cmd[0]== "OPER")
	  oper();
	else if (_parsed_cmd[0]== "KILL" || _parsed_cmd[0] == "kill")
	  kill();
	else if (_parsed_cmd[0]== "MODE")
		mode();
	else if (_parsed_cmd[0]== "KICK")
		kick();
	else if (_parsed_cmd[0]== "INVITE")
		invite();
	else if (_parsed_cmd[0]== "NOTICE")
		notice();
	else
		return;
}

void
Client::parse_lines(std::string buff)
{
	std::stringstream			strstream(buff);
	std::vector<std::string>	buff_lines;
	std::string 				line;

	while(getline(strstream, line, '\n'))
	{
		if (*(line.end() - 1) == '\n')
			line.erase(line.end() - 1);
		if (*(line.end() - 1) == '\r')
			line.erase(line.end() - 1);
		buff_lines.push_back(line);
	}

	for (std::vector<std::string>::iterator it = buff_lines.begin(); it != buff_lines.end(); it++)
		parse_command(*it);
}

void
Client::parse_command(std::string command)
{
	std::stringstream	strstream(command);
	std::string 		word;
	std::string			msg;
	bool				last_arg = false;

	if (!_parsed_cmd.empty())
		_parsed_cmd.clear();

	while(getline(strstream, word, ' '))
	{
		if (word[0] == ':')
		{
			last_arg = true;
			word.erase(word.begin());
		}
		if (last_arg == false) {
			if (word.length() > 0)
				_parsed_cmd.push_back(word);
		}
		else
			msg += word + " ";
	}
	if (!msg.empty() && msg.length() > 0)
		_parsed_cmd.push_back(msg);

	search_command();
}


// void
// Client::clear_client()
// {
// 	_nick.clear();
// 	_user.clear();
// 	_client_id_str.clear();
// 	_data_connexion.clear();
// 	// _buff.clear();
// 	_parsed_cmd.clear();
// 	_server = NULL;
// 	// delete this;
// }

void
Client::delete_client_from_chans()
{
	for (std::map<std::string, Channel>::iterator chan = _server->_channels.begin(); chan != _server->_channels.end(); chan++)
	{
		if (chan->second._clients.find(_client_id) != chan->second._clients.end())
		{
			erase_from_chan_opers(chan->second._id_operators, _client_id);
			chan->second._clients.erase(_client_id);
			chan->second._first_connexion.erase(chan->second._first_connexion.find(_client_id));
			//msg aux autres clients du channel
			for (std::map<int, Client*>::iterator client = _server->_channels.find(chan->first)->second._clients.begin();
				client != _server->_channels.find(chan->first)->second._clients.end(); client++)
			{
				client->second->_msg_buffer += RPL_QUIT(USER_ID2(_nick, _user), _quit_msg);
			}
		}
	}
}

void
Client::delete_client()
{
	std::vector<struct pollfd>::iterator it = _server->_pfds.begin();
	std::vector<struct pollfd>::iterator end = _server->_pfds.end();
	for (; it != end; it++)
	{
		if (it->fd == _client_id)
			_server->_pfds.erase(it);
	}
	delete_client_from_chans();
	_server->_clients.erase(_client_id);
}
