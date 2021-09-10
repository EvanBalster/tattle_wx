#include <sstream>

#include "key_store.h"


using namespace tattle;



CookieStore::DecodeResult CookieStore::decode(const std::string &marshalled)
{
	auto i = marshalled.begin(), e = marshalled.end();

	enum PARSE_STATE {NAME, VALUE, MISC};
	PARSE_STATE part = NAME;

	DecodeResult result = {0,0};

	while (i < e)
	{
		std::string name;
		while (i < e) switch (*i)
		{
		case int('='):
			++i;
			goto name_done;

		default:
			if (*i > 0x1F) {name.push_back(*i); ++i; break;}
			else           [[fallthrough]];

		case 0x7F:
		case int(';'):
		case int(' '):  case int('\r'): case int('\f'):
		case int('\n'): case int('\t'): case int('\v'):
			name.clear();
			goto name_done;

		
		}
	name_done:

		std::string value;
		if (name.length()) while (i < e) switch (*i)
		{
		case int('='):
			++i;
			goto value_done;

		default:
			if (*i > 0x1F) {value.push_back(*i); ++i; break;}
			else           [[fallthrough]];

		case 0x7F:
		case int('\r'): case int('\n'): case int('\v'):
			value.clear();
			goto value_done;
		}
	value_done:

		// Set value
		if (name.length())
		{
			if (set(name, value)) ++result.parsed;
			else                  ++result.errors;
		}
		else ++result.errors;

		// Skip to next line.
		while (i < e && *i != '\n') ++i;
		if (i < e) ++i;
	}


	return result;
}
std::string CookieStore::encode() const
{
	std::stringstream ss(std::ios_base::out | std::ios_base::binary);

	for (auto &pair : _map)
	{
		ss << pair.first << '=' << pair.second << '\n';
	}

	return ss.str();
}

bool CookieStore::set(const std::string &name, const std::string &value)
{
	if (name.length() == 0)
		return false;

	if (name.length() > 256 || value.length() > 4096)
		return false;

	for (auto c : name) switch (c)
	{
		case 0x7F:
		case int('='):
		case int(';'):
		case int(' '):  case int('\r'): case int('\f'):
		case int('\n'): case int('\t'): case int('\v'):
			return false;
		default:
			break;
	}

	std::string cleanValue;
	cleanValue.reserve(value.size());

	for (auto c : value) switch (c)
	{
		case int(';'): case 0x7F:
			return false;
		case int('\r'): case int('\n'): case int('\v'):
			c = ' ';
			[[fallthrough]];
		default:
			cleanValue.push_back(c);
			break;
	}

	_map[name] = cleanValue;
	return true;
}
