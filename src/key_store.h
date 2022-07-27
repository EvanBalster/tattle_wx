#pragma once


#include <unordered_map>
#include <string>


namespace tattle
{
	class CookieStore
	{
	public:
		using Map = std::unordered_map<std::string, std::string>;

		struct DecodeResult
		{
			size_t parsed;
			size_t errors;

			explicit operator bool() const noexcept    {return parsed > 0;}
		};


	public:
		// Access internal map (read-only)
		const Map &map() const    {return _map;}


		// Encode/decode as file contents
		DecodeResult decode(const std::string &marshalled);
		std::string  encode() const;

		bool        has(const std::string &name)                              const noexcept    {return _map.count(name) != 0;}
		std::string get(const std::string &name, const std::string &def = "") const noexcept    {auto p=_map.find(name); return (p==_map.end()) ? def : p->second;}

		/*
			Set keys to values.
				name may not contain whitespace, semicolon or equals-sign
				value may not contain semicolon; linebreaks will turn into spaces.
				control characters 0x00 to 0x1F and 0x7F are not allowed.
		*/
		bool set(const std::string &name, const std::string &value);
		void set(const CookieStore::Map &other)                           {for (auto &pair : other) set(pair.first, pair.second);}
		void set(const CookieStore      &other)                           {set(other.map);}


	private:
		Map _map;
	};
}


/*
	IMPLEMENTATION
*/