#include <fstream>
#include <sstream>

#include "tattle.h"

#include <wx/cmdline.h>


#ifndef TATTLE_LEGACY_COMMAND_LINE
	#define TATTLE_LEGACY_COMMAND_LINE 1
#endif


#define CMD_HELP(SHORT, LONG, DESC)              { wxCMD_LINE_SWITCH, SHORT, LONG, DESC, wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
#define CMD_SWITCH(SHORT, LONG, DESC)            { wxCMD_LINE_SWITCH, SHORT, LONG, DESC, wxCMD_LINE_VAL_NONE },
#define CMD_OPTION_STRING(SHORT, LONG, DESC)     { wxCMD_LINE_OPTION, SHORT, LONG, DESC, wxCMD_LINE_VAL_STRING }, 
#define CMD_OPTION_STRINGS(SHORT, LONG, DESC)    { wxCMD_LINE_OPTION, SHORT, LONG, DESC, wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE }, 
#define CMD_REQUIRE_STRING(SHORT, LONG, DESC)    { wxCMD_LINE_OPTION, SHORT, LONG, DESC, wxCMD_LINE_VAL_STRING, wxCMD_LINE_OPTION_MANDATORY },
#define CMD_OPTION_INT(SHORT, LONG, DESC)        { wxCMD_LINE_OPTION, SHORT, LONG, DESC, wxCMD_LINE_VAL_NUMBER },
#define CMD_PARAMETERS(SHORT, LONG, DESC)        { wxCMD_LINE_PARAM,  SHORT, LONG, DESC, wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE | wxCMD_LINE_PARAM_OPTIONAL},

namespace tattle
{
	static const wxCmdLineEntryDesc g_cmdLineDesc[] =
	{
		/*
			COMMAND-LINE OPTIONS.
		*/

		CMD_HELP          ("h", "help",           "Displays help on command-line parameters.")

		CMD_OPTION_STRINGS("D",  "dump",          "<fname>  (Debug) Dump full configuration to a JSON file.")

#if TATTLE_LEGACY_COMMAND_LINE
		CMD_OPTION_STRINGS("c",  "config-file",   "<fname>  Config file with more command-line arguments.")
		CMD_OPTION_STRINGS("cs", "cookie-store",  "<fname>  Saves user consent, stored inputs & cookies.")
		CMD_OPTION_STRINGS("ct", "category",      "<type>:<id>")

		CMD_OPTION_STRING ("up", "url-post",      "<url>    HTTP URL for posting report. Either -up or -uq is required.")
		CMD_OPTION_STRING ("uq", "url-query",     "<url>    HTTP URL for pre-query.")

		CMD_SWITCH        ("s",  "silent",        "Bypass prompt; upload without user input.")
		CMD_SWITCH        ("sq", "silent-query",  "Bypass prompt for user consent to query.")
		
		CMD_SWITCH        ("wt", "stay-on-top",   "Make the Tattle GUI stay on top.")
		CMD_SWITCH        ("wp", "show-progress", "Show progress bar dialogs.")
		CMD_OPTION_STRING ("wi", "icon",          "Set a standard icon: information, warning or error.")
		//CMD_SWITCH       ("w", "parent-window",   "Make the prompt GUI stay on top.")
		
		CMD_OPTION_STRINGS("l",  "log",           "<fname>        Append information to log file.")

		CMD_OPTION_STRINGS("a",  "arg",           "<key>=<value>  Argument string.")
		CMD_OPTION_STRINGS("aq", "arg-query",     "<key>=<value>  Argument string used in pre-query.")
		
		CMD_OPTION_STRINGS("ft", "file",          "<key>=<fname>  Argument text file.")
		CMD_OPTION_STRINGS("fb", "file-binary",   "<key>=<fname>  Argument file, binary.")
		
		CMD_OPTION_STRINGS("tb", "trunc-begin",   "<key>=<N>      Attach first N bytes of file.")
		CMD_OPTION_STRINGS("te", "trunc-end",     "<key>=<N>      Attach last N bytes of file.")
		CMD_OPTION_STRINGS("tn", "trunc-note",    "<key>=<text>   Line of text marking truncation.")

		CMD_OPTION_STRING ("pt", "title",         "<title>        Title of the prompt window.")
		CMD_OPTION_STRING ("pm", "message",       "<message>      A header message summarizing the prompt.")
		CMD_OPTION_STRING ("px", "technical",     "<plaintext>    A technical summary message.")
		CMD_OPTION_STRING ("ps", "label-send",    "<label>        Text of the 'send' button.")
		CMD_OPTION_STRING ("pc", "label-cancel",  "<label>        Text of the 'cancel' button.")
		CMD_OPTION_STRING ("pv", "label-view",    "<label>        Text of the 'view data' button.")
		
		CMD_OPTION_STRINGS("i",  "field",         "<key>=<label>  Single-line field for user input.")
		CMD_OPTION_STRINGS("im", "field-multi",   "<key>=<label>  Multi-line field.")
		CMD_OPTION_STRINGS("id", "field-default", "<key>=<value>  Hint message for -uf field.")
		CMD_OPTION_STRINGS("ih", "field-hint",    "<key>=<hint>   Default value for a field.")
		CMD_OPTION_STRINGS("iw", "field-warning", "<key>=<text>   Show a warning if field is left empty.")
		CMD_OPTION_STRING ("is", "field-store",   "<key>          Save and restore the user's input.")

		//CMD_OPTION_STRINGS("ts", "tech-string",   "<label>:<value>      Technical info string.")
		//CMD_OPTION_STRINGS("tf", "tech-file",     "<label>:<fname>      Technical info as a linked file.")
		CMD_SWITCH        ("v" , "view-data",     "               Enable 'view data' dialog.")
		CMD_OPTION_STRINGS("vd", "view-dir",      "<path>         Folder listed in 'view data' dialog.")
#endif

		CMD_PARAMETERS(nullptr, nullptr, "JSON Configuration File.")

		{wxCMD_LINE_NONE}
	};

	static bool ParsePair(const wxString &str, wxString &first, wxString &second)
	{
		wxString::size_type p = str.find_first_of('=');
		if (p == wxString::npos || p == 0) return false;

		first = str.substr(0, p);
		second = str.substr(p + 1);
		return true;
	}

	enum CMD_LINE_ERR
	{
		CMD_ERR_NONE = 0,
		CMD_ERR_UNKNOWN,
		CMD_ERR_BAD_PAIR,
		CMD_ERR_CONTENT_REDECLARED,
		CMD_ERR_CONTENT_MISSING,
		CMD_ERR_CONTENT_NOT_APPLICABLE,
		CMD_ERR_FAILED_TO_OPEN_CONFIG,
		CMD_ERR_FAILED_TO_OPEN_FILE,
		CMD_ERR_BAD_URL,
		CMD_ERR_BAD_SIZE,
	};

	void Tattle_OnInitCmdLine(wxCmdLineParser& parser)
	{
		parser.SetDesc(g_cmdLineDesc);
	}

	bool Tattle_ExecCmdLine(Json& config, wxCmdLineParser& parser);
	
	bool Tattle_ExecCmdLineArg(Json &config, const wxCmdLineArg &arg)
	{
		bool success = true;
		
		wxString argName = arg.GetShortName();
		char
			c0 = char(argName[0]),
			c1 = ((argName.length() > 1) ? char(argName[1]) : '\0');
		//	c2 = ((argName.length() > 2) ? char(argName[2]) : '\0');
		
		CMD_LINE_ERR err = CMD_ERR_NONE;
		
		//cout << ">>>>" << argName << " " << arg.GetStrVal() << endl;;

		switch (c0)
		{
		case int('D'):
			config["path"]["config_dump"] = std::string(arg.GetStrVal().ToUTF8());
			break;

#if TATTLE_LEGACY_COMMAND_LINE
		case int('l'):
			if (c1 == 0)
				config["path"]["log"] = arg.GetStrVal();
			else
				err = CMD_ERR_UNKNOWN;
			break;

		case int('u'):
			{
				/*
					URLs
				*/
				if (c1 == 'p')
				{
					config["service"]["url"]["post"] = std::string(arg.GetStrVal());
					//if (!url_post().isSet()) err = CMD_ERR_BAD_URL;
				}
				else if (c1 == 'q')
				{
					config["service"]["url"]["query"] = std::string(arg.GetStrVal());
					//if (!url_query().isSet()) err = CMD_ERR_BAD_URL;
				}
				else err = CMD_ERR_UNKNOWN;
			}
			break;

		case int('s'):
			{
				switch (c1)
				{
				case 0:
					uiConfig.config["gui"]["query"] = "silent";
					uiConfig.config["gui"]["post"] = "silent";
					break;
				case int('q'): uiConfig.config["gui"]["query"] = "silent"; break;
				case int('p'): uiConfig.config["gui"]["post"]  = "silent"; break;
				default:
					err = CMD_ERR_UNKNOWN;
				}
			}
			break;

		case int('w'):
			{
				switch (c1)
				{
				case int('t'): config["gui"]["stay_on_top"] = true; break;
				case int('p'): config["gui"]["progress_bar"] = true; break;
				case int('i'):
					{
					config["gui"]["icon"] = arg.GetStrVal();
						wxArtID id = uiConfig.GetIconID(arg.GetStrVal());
						if (!id.length()) err = CMD_ERR_CONTENT_NOT_APPLICABLE;
					}
					break;
				default:
					err = CMD_ERR_UNKNOWN;
				}
			}
			break;

		case int('c'):
			{
				switch (c1)
				{
				case 0:
					{
						wxFile file(arg.GetStrVal());
						if (file.IsOpened())
						{
							// Read in the config file
							wxString configFile;
							file.ReadAll(&configFile);

							// Replace newlines with spaces
							configFile.Replace(wxT("\r"), wxT(" "));
							configFile.Replace(wxT("\n"), wxT(" "));

							// Run the parser (with help option disabled)
							wxCmdLineParser parser(configFile);
							parser.SetDesc(g_cmdLineDesc+1);
							parser.Parse();

							success &= Tattle_ExecCmdLine(config, parser);
						}
						else err = CMD_ERR_FAILED_TO_OPEN_CONFIG;
					}
					break;

				case int('s'):
					config["path"]["state"] = std::string(arg.GetStrVal().ToUTF8());
					break;

				case int('t'):
					{
						Report::Identifier id(std::string(arg.GetStrVal()));

						config.merge_patch(
						{{"report", {
							{"type", id.type},
							{"id",   id.id}
						} }});
					}
					break;

				default:
					err = CMD_ERR_UNKNOWN;
				}
				
			}
			break;

		case int('a'):
			{
				// Argument string
				wxString name, value;
				if (!ParsePair(arg.GetStrVal(), name, value))    {err = CMD_ERR_BAD_PAIR; break;}
				if (!name.length() || name[0] == '$')            {err = CMD_ERR_BAD_PAIR; break;}

				//Content must not exist.
				if (config["report"]["contents"].contains(name) ||
					config["report"]["query"].contains(name))
						{err = CMD_ERR_CONTENT_REDECLARED; break;}

				switch (c1)
				{
				case 0:
					config["report"]["contents"][std::string(name)] = value;
					break;
				case int('q'):
					config["report"]["query"][std::string(name)] = value;
					break;
				default:
					err = CMD_ERR_UNKNOWN;
					break;
				}
				if (err) break;
			}
			break;

		case int('f'):
			{
				// Argument string
				wxString name, file_path;
				if (!ParsePair(arg.GetStrVal(), name, file_path))    {err = CMD_ERR_BAD_PAIR; break;}
				if (!name.length() || name[0] == '$')                {err = CMD_ERR_BAD_PAIR; break;}

				//Content must not exist.
				if (config["report"]["contents"].contains(name) ||
					config["report"]["query"].contains(name))
						{err = CMD_ERR_CONTENT_REDECLARED; break;}

				std::string content_type;
				std::string content_transfer_encoding;
				switch (c1)
				{
				case int('t'):
					content_type = "text/plain";
					break;
				case int('b'):
					content_type = "application/octet-stream";
					//content_transfer_encoding = "Base64";
					break;
				default:
					err = CMD_ERR_UNKNOWN;
					break;
				}

				if (!wxFile::Access(file_path, wxFile::read))
				{
					// Fail gentry.
					err = CMD_ERR_FAILED_TO_OPEN_FILE;
				}
				else
				{
					// Probe the file...
					wxFile file(file_path);
					if (!file.IsOpened()) err = CMD_ERR_FAILED_TO_OPEN_FILE;
					file.Close();
				}

				if (err == CMD_ERR_NONE)
				{
					Json &content = config["report"]["contents"][std::string(name)];
					content =
					{
						{"path", file_path},
						{"content-type", content_type}
					};
					if (content_transfer_encoding.length())
						content["content-transfer-encoding"] = content_transfer_encoding;
				}
			}
			break;

		case int('t'):
			{
				// Argument string
				wxString name, value;
				if (!ParsePair(arg.GetStrVal(), name, value)) { err = CMD_ERR_BAD_PAIR; break; }
				
				//Content must exist and must be a file
				Json &content = config["report"]["contents"][std::string(name)];
				if (!content.is_object())         { err = CMD_ERR_CONTENT_MISSING; break; }
				if (!content.contains("path"))    { err = CMD_ERR_CONTENT_NOT_APPLICABLE; break; }

				if (!content.contains("truncate")
					|| !content["truncate"].is_array())
				{
					content["truncate"] = Json::array_t{0, 0, "(trimmed)"};
				}
				
				if (c1 == 'b' || c1 == 'e')
				{
					long bytes = 0;
					if (value.ToLong(&bytes) && bytes >= 0)
					{
						content["truncate"][(c1 == 'b') ? 0 : 1] = unsigned(bytes);
					}
					else err = CMD_ERR_BAD_SIZE;
				}
				else if (c1 == 'n')
				{
					content["truncate"][2] = value;
				}
				else err = CMD_ERR_UNKNOWN;
			}
			break;

		case int('p'):
			{
				//Prompt stuff
				switch (c1)
				{
				case int('t'): config["text"]["prompt"]["title"] = arg.GetStrVal(); break;
				case int('m'): config["text"]["prompt"]["message"] = arg.GetStrVal(); break;
				case int('x'): config["report"]["summary"] = arg.GetStrVal(); break;
				case int('s'): config["text"]["prompt"]["btn_send"] = arg.GetStrVal(); break;
				case int('c'): config["text"]["prompt"]["btn_cancel"] = arg.GetStrVal(); break;
				case int('v'): config["text"]["prompt"]["btn_review"] = arg.GetStrVal(); break;
				default: err = CMD_ERR_UNKNOWN;
				}
				
			}
			break;

		case int('i'):
			{
				if (c1 == '\0' || c1 == 'm')
				{
					wxString name, label;
					if (!ParsePair(arg.GetStrVal(), name, label)) { err = CMD_ERR_BAD_PAIR; break; }

					//Content must not exist.
					if (config["report"]["contents"].contains(name) ||
						config["report"]["query"].contains(name))
							{err = CMD_ERR_CONTENT_REDECLARED; break;}

					const char *input_type = nullptr;
					if     (c1 == '\0') input_type = "text";
					else if (c1 == 'm') input_type = "multiline";
					else {err = CMD_ERR_UNKNOWN; break;}

					auto &content = config["report"]["contents"][std::string(name)];
					content =
					{
						{"input", input_type},
						{"label", label}
					};
				}
				else if (c1 == 's')
				{
					wxString name = arg.GetStrVal();

					//Content must exist and must be an input.
					Json &content = config["report"]["contents"][std::string(name)];
					if (!content.is_object())         { err = CMD_ERR_CONTENT_MISSING; break; }
					if (!content.contains("input"))   { err = CMD_ERR_CONTENT_NOT_APPLICABLE; break; }

					content["persist"] = true;
				}
				else if (c1 == 'h' || c1 == 'd' || c1 == 'w')
				{
					wxString name, value;
					if (!ParsePair(arg.GetStrVal(), name, value)) { err = CMD_ERR_BAD_PAIR; break; }

					//Content must exist and must be an input.
					Json &content = config["report"]["contents"][std::string(name)];
					if (!content.is_object())         { err = CMD_ERR_CONTENT_MISSING; break; }
					if (!content.contains("input"))   { err = CMD_ERR_CONTENT_NOT_APPLICABLE; break; }

					switch (c1)
					{
					case int('h'): //Add a hint.
						if (content["input"] != "text") err = CMD_ERR_CONTENT_NOT_APPLICABLE;
						content["placeholder"] = value;
						break;

					case int('d'): // Set default value.
						content["value"] = value;
						break;

					case int('w'):
						content["input_warning"] = value;
						break;

					default: err = CMD_ERR_UNKNOWN;
					}
				}
				else
				{
					err = CMD_ERR_UNKNOWN;
				}
			}
			break;

		case int('v'):
			switch (c1)
			{
			case 0: // -v enables review
				config["gui"]["prompt"]["review"] = true;
				break;

			case int('d'): // -vd: review directory
				config["gui"]["prompt"]["review"] = true;
				config["path"]["review"] = arg.GetStrVal();
				break;

			default:
				err = CMD_ERR_UNKNOWN;
			}
			break;
#endif

		default:
			err = CMD_ERR_UNKNOWN;
		}

		/*
		*	Handle errors.
		*/
		switch (err)
		{
		case CMD_ERR_NONE:
			// OK!
			break;
		case CMD_ERR_UNKNOWN:
			cout << "Unknown argument: `" << argName << "'" << endl;
			//success = false;
			break;
		case CMD_ERR_BAD_PAIR:
			cout << "Malformed pair: `-" << argName << " " << arg.GetStrVal()
				<< "' -- should be \"<first_part>=<second_part>\" (without pointy brackets)" << endl;
			//success = false;
			break;
		case CMD_ERR_BAD_URL:
			cout << "Malformed URL: `-" << argName << " " << arg.GetStrVal()
				<< "' -- should be \"http://<domain>[/path...]\".  No HTTPS." << endl;
			success = false;
			break;
		case CMD_ERR_BAD_SIZE:
			cout << "Malformed Size: `-" << argName << " " << arg.GetStrVal()
				<< "' -- should be an unsigned integer." << endl;
			success = false;
			break;
		case CMD_ERR_CONTENT_REDECLARED:
		case CMD_ERR_CONTENT_MISSING:
		case CMD_ERR_CONTENT_NOT_APPLICABLE:
			{
				if (err == CMD_ERR_CONTENT_REDECLARED)
					cout << "Specified content more than once: `-" << argName << " ";
				else if (err == CMD_ERR_CONTENT_MISSING)
					cout << "Content was not declared first: `-" << argName << " ";
				else if (err == CMD_ERR_CONTENT_NOT_APPLICABLE)
					cout << "Not applicable to this type of content: `-" << argName << " ";

				cout << arg.GetStrVal() << "'" << endl;
				//success = false;
			}
			break;
		case CMD_ERR_FAILED_TO_OPEN_CONFIG:
		{
			cout << "Failed to open Tattle config: `-" << argName << arg.GetStrVal() << "'" << endl;
			//success = false;
		}
		case CMD_ERR_FAILED_TO_OPEN_FILE:
			{
				cout << "Failed to open file: `-" << argName << arg.GetStrVal() << "'" << endl;
				//success = false;
			}
			break;
		}
		
		return success;
	}

	bool Tattle_ExecCmdLine(Json& config, wxCmdLineParser& parser)
	{
		wxCmdLineArgs args = parser.GetArguments();

		bool success = true;

		for (wxCmdLineArgs::const_iterator i = args.begin(); i != args.end(); ++i)
		{
			if (i->GetKind() == wxCMD_LINE_PARAM)
			{
				wxString path = i->GetStrVal();

				if (!wxFile::Exists(path)) return false;

				wxFile file(path);
				if (!file.IsOpened()) return false;

				try
				{
					Json json;

					wxString contents_str;
					file.ReadAll(&contents_str);
					file.Close();

					const auto text = contents_str.ToUTF8();

					config.merge_patch(
						Json::parse(text.data(), text.data() + text.length(),
							nullptr, true, true));
				}
				catch (Json::parse_error& e)
				{
					std::stringstream ss;
					ss << "Failed to read configuration `" << path << "' : " << e.what();
					std::cout << ss.str() << std::endl;
					wxMessageBox(ss.str(), "Problem while making a report");
				}
				catch (...)
				{
					std::stringstream ss;
					ss << "Failed to read configuration `" << path << "' : unknown exception";
					std::cout << ss.str() << std::endl;
					wxMessageBox(ss.str(), "Problem while making a report");
				}
			}
			else
			{
				success &= Tattle_ExecCmdLineArg(config, *i);
			}
		}

		return success;
	}
}