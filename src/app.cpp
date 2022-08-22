//
//  app.cpp
//  tattle
//
//  Created by Evan Balster on 11/12/16.
//

#include "tattle.h"

#include <wx/app.h>
#include <wx/cmdline.h>

//File reading / encoding
#include <wx/file.h>


#define CMD_HELP(SHORT, LONG, DESC)              { wxCMD_LINE_SWITCH, SHORT, LONG, DESC, wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
#define CMD_SWITCH(SHORT, LONG, DESC)            { wxCMD_LINE_SWITCH, SHORT, LONG, DESC, wxCMD_LINE_VAL_NONE },
#define CMD_OPTION_STRING(SHORT, LONG, DESC)     { wxCMD_LINE_OPTION, SHORT, LONG, DESC, wxCMD_LINE_VAL_STRING }, 
#define CMD_OPTION_STRINGS(SHORT, LONG, DESC)    { wxCMD_LINE_OPTION, SHORT, LONG, DESC, wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE }, 
#define CMD_REQUIRE_STRING(SHORT, LONG, DESC)    { wxCMD_LINE_OPTION, SHORT, LONG, DESC, wxCMD_LINE_VAL_STRING, wxCMD_LINE_OPTION_MANDATORY },
#define CMD_OPTION_INT(SHORT, LONG, DESC)        { wxCMD_LINE_OPTION, SHORT, LONG, DESC, wxCMD_LINE_VAL_NUMBER },

namespace tattle
{
	static const wxCmdLineEntryDesc g_cmdLineDesc[] =
	{
		/*
			COMMAND-LINE OPTIONS.
		*/

		CMD_HELP          ("h", "help",           "Displays help on command-line parameters.")

		CMD_OPTION_STRINGS("c",  "config-file",   "<fname>  Config file with more arguments.")
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

		{wxCMD_LINE_NONE}
	};

	class TattleApp;
}

using namespace tattle;

static PersistentData persist_;
static Report   report_;
static UIConfig uiConfig_ = UIConfig(report_.config);

PersistentData &tattle::persist = persist_;
const Report   &tattle::report   = report_;
const UIConfig &tattle::uiConfig = uiConfig_;

UIConfig::UIConfig(Json & _config) :
	config(_config)
{
}

unsigned UIConfig::marginSm() const    {return JsonFetch(config, "/style/margin_small", 5u);}
unsigned UIConfig::marginMd() const    {return JsonFetch(config, "/style/margin_medium", 8u);}
unsigned UIConfig::marginLg() const    {return JsonFetch(config, "/style/margin_large", 10u);}

wxArtID UIConfig::GetIconID(wxString tattleName) const
{
	if (tattleName == "information") return wxART_INFORMATION;
	if (tattleName == "info")        return wxART_INFORMATION;
	if (tattleName == "warning")     return wxART_WARNING;
	if (tattleName == "error")       return wxART_ERROR;
	if (tattleName == "question")    return wxART_QUESTION;
	if (tattleName == "help")        return wxART_HELP;
	if (tattleName == "tip")         return wxART_TIP;
	return "";
}

static TattleApp *tattleApp = NULL;


class tattle::TattleApp : public wxApp
{
public:
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
		CMD_ERR_PARAM_REDECLARED,
		CMD_ERR_PARAM_MISSING,
		CMD_ERR_PARAM_NOT_APPLICABLE,
		CMD_ERR_FAILED_TO_OPEN_CONFIG,
		CMD_ERR_FAILED_TO_OPEN_FILE,
		CMD_ERR_BAD_URL,
		CMD_ERR_BAD_SIZE,
	};

	virtual void OnInitCmdLine(wxCmdLineParser& parser) wxOVERRIDE
	{
		parser.SetDesc(g_cmdLineDesc);
	}
	
	bool ExecCmdLineArg(const wxCmdLineArg &arg)
	{
		bool success = true;
		
		wxString argName = arg.GetShortName();
		char
			c0 = char(argName[0]),
			c1 = ((argName.length() > 1) ? char(argName[1]) : '\0');
		//	c2 = ((argName.length() > 2) ? char(argName[2]) : '\0');
		
		CMD_LINE_ERR err = CMD_ERR_NONE;


		auto &rpJson = report_.config;
		auto &uiJson = uiConfig_.config;
		
		//cout << ">>>>" << argName << " " << arg.GetStrVal() << endl;;

		switch (c0)
		{
		case int('l'):
			if (c1 == 0)
				report_.config["path"]["log"] = arg.GetStrVal();
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
					report_.config["url"]["post"] = std::string(arg.GetStrVal());
					if (!report_.url_post().isSet()) err = CMD_ERR_BAD_URL;
				}
				else if (c1 == 'q')
				{
					report_.config["url"]["query"] = std::string(arg.GetStrVal());
					if (!report_.url_query().isSet()) err = CMD_ERR_BAD_URL;
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
				case int('t'): uiJson["gui"]["stay_on_top"] = true; break;
				case int('p'): uiJson["gui"]["progress_bar"] = true; break;
				case int('i'):
					{
						uiJson["gui"]["icon"] = arg.GetStrVal();
						wxArtID id = uiConfig.GetIconID(arg.GetStrVal());
						if (!id.length()) err = CMD_ERR_PARAM_NOT_APPLICABLE;
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

							success &= OnCmdLineParsed(parser);
						}
						else err = CMD_ERR_FAILED_TO_OPEN_CONFIG;
					}
					break;

				case int('s'):
					if (!persist.load(arg.GetStrVal()))
						err = CMD_ERR_FAILED_TO_OPEN_FILE;
					break;

				case int('t'):
					{
						Report::Identifier id(std::string(arg.GetStrVal()));

						report_.config.merge_patch(
						{{"data", {
							{"$type", id.type},
							{"$id",   id.id}
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
				if (!ParsePair(arg.GetStrVal(), name, value)) { err = CMD_ERR_BAD_PAIR; break; }

				//Parameter must not exist.
				if (report_.findParam(name) != NULL) { err = CMD_ERR_PARAM_REDECLARED; break; }

				Report::Parameter param;
				param.type = PARAM_STRING;
				param.name = name;

				switch (c1)
				{
				case 0: break;
				case int('q'): param.preQuery = true; break;
				default:
					err = CMD_ERR_UNKNOWN;
					break;
				}
				if (err) break;

				param.json =
				{
					{"value", value}
				};
				report_.params.push_back(std::move(param));
			}
			break;

		case int('f'):
			{
// Argument string
				wxString name, value;
				if (!ParsePair(arg.GetStrVal(), name, value)) { err = CMD_ERR_BAD_PAIR; break; }

				//Parameter must not exist.
				if (report_.findParam(name) != NULL) { err = CMD_ERR_PARAM_REDECLARED; break; }
					
				Report::Parameter param;
				if      (c1 == 't') {param.type = PARAM_FILE_TEXT;}
				else if (c1 == 'b') {param.type = PARAM_FILE_BIN; }
				else                {err = CMD_ERR_UNKNOWN; break;}

				std::string content_info;

				if (!wxFile::Access(value, wxFile::read))
				{
					// Fail gentry.
					err = CMD_ERR_FAILED_TO_OPEN_FILE;
				}
				else
				{
					// Probe the file...
					wxFile file(value);
					if (file.IsOpened())
					{
						if (param.type == PARAM_FILE_TEXT)
						{
							// Content info
							content_info = "Content-Type: text/plain";
						}
						else
						{
							// Content info
							content_info = "Content-Type: application/octet-stream"
								"\r\n"
								"Content-Transfer-Encoding: Base64";
						}
					}
					else err = CMD_ERR_FAILED_TO_OPEN_FILE;
				}

				if (err == CMD_ERR_NONE)
				{
					param.json =
					{
						{"path", value},
						{"content_info", content_info}
					};

					param.name = name;

					report_.params.push_back(std::move(param));
				}
			}
			break;

		case int('t'):
			{
				// Argument string
				wxString name, value;
				if (!ParsePair(arg.GetStrVal(), name, value)) { err = CMD_ERR_BAD_PAIR; break; }
				
				//Parameter must exist and must be a file
				Report::Parameter *param = report_.findParam(name);
				if (param == NULL)
					{ err = CMD_ERR_PARAM_MISSING; break; }
				if (param->type != PARAM_FILE_BIN && param->type != PARAM_FILE_TEXT)
					{ err = CMD_ERR_PARAM_NOT_APPLICABLE; break; }

				if (!param->json.contains("truncate")
					|| !param->json["truncate"].is_array())
				{
					param->json["truncate"] = Json::array_t{0, 0, "(trimmed)"};
				}
				
				if (c1 == 'b' || c1 == 'e')
				{
					long bytes = 0;
					if (value.ToLong(&bytes) && bytes >= 0)
					{
						param->json["truncate"][(c1 == 'b') ? 0 : 1] = unsigned(bytes);
					}
					else err = CMD_ERR_BAD_SIZE;
				}
				else if (c1 == 'n')
				{
					param->json["truncate"][2] = value;
				}
				else err = CMD_ERR_UNKNOWN;
			}
			break;

		case int('p'):
			{
				//Prompt stuff
				switch (c1)
				{
				case int('t'): uiJson["gui"]["title"] = arg.GetStrVal(); break;
				case int('m'): uiJson["gui"]["message"] = arg.GetStrVal(); break;
				case int('x'): uiJson["gui"]["technical"] = arg.GetStrVal(); break;
				case int('s'): uiJson["gui"]["label_send"] = arg.GetStrVal(); break;
				case int('c'): uiJson["gui"]["label_cancel"] = arg.GetStrVal(); break;
				case int('v'): uiJson["gui"]["label_review"] = arg.GetStrVal(); break;
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

					//Parameter must not exist.
					if (report_.findParam(name) != NULL) { err = CMD_ERR_PARAM_REDECLARED; break; }

					Report::Parameter param;
					param.name = name;
					param.json =
					{
						{"label", label}
					};
					if (c1 == '\0')      param.type = PARAM_FIELD;
					else if (c1 == 'm') param.type = PARAM_FIELD_MULTI;
					report_.params.push_back(std::move(param));
				}
				else if (c1 == 's')
				{
					wxString name = arg.GetStrVal();

					// Value must exist
					Report::Parameter *param = report_.findParam(name);
					if (param == NULL) { err = CMD_ERR_PARAM_MISSING; break; }

					param->json["persist"] = true;
				}
				else if (c1 == 'h' || c1 == 'd' || c1 == 'w')
				{
					wxString name, value;
					if (!ParsePair(arg.GetStrVal(), name, value)) { err = CMD_ERR_BAD_PAIR; break; }

					// Value must exist
					Report::Parameter *param = report_.findParam(name);
					if (param == NULL) { err = CMD_ERR_PARAM_MISSING; break; }

					switch (c1)
					{
					case int('h'): //Add a hint.
						if (param->type != PARAM_FIELD) err = CMD_ERR_PARAM_NOT_APPLICABLE;
						param->json["placeholder"] = value;
						break;

					case int('d'): // Set default value.
						if (param->type != PARAM_FIELD && param->type != PARAM_FIELD_MULTI) err = CMD_ERR_PARAM_NOT_APPLICABLE;
						param->json["value"] = value;
						break;

					case int('w'):
						if (param->type != PARAM_FIELD && param->type != PARAM_FIELD_MULTI) err = CMD_ERR_PARAM_NOT_APPLICABLE;
						param->json["input_warning"] = value;
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
				uiJson["gui"]["review"] = true;
				break;

			case int('d'): // -vd: review directory
				uiJson["gui"]["review"] = true;
				rpJson["path"]["review"] = arg.GetStrVal();
				break;

			default:
				err = CMD_ERR_UNKNOWN;
			}
			break;

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
		case CMD_ERR_PARAM_REDECLARED:
		case CMD_ERR_PARAM_MISSING:
		case CMD_ERR_PARAM_NOT_APPLICABLE:
			{
				if (err == CMD_ERR_PARAM_REDECLARED)
					cout << "Specified parameter more than once: `-" << argName << " ";
				else if (err == CMD_ERR_PARAM_MISSING)
					cout << "Field must be declared first: `-" << argName << " ";
				else if (err == CMD_ERR_PARAM_NOT_APPLICABLE)
					cout << "Property is not applicable to this field: `-" << argName << " ";

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
	
	virtual bool OnCmdLineParsed(wxCmdLineParser& parser) wxOVERRIDE
	{
		wxCmdLineArgs args = parser.GetArguments();

		bool success = true;

		for (wxCmdLineArgs::const_iterator i = args.begin(); i != args.end(); ++i)
		{
			success &= ExecCmdLineArg(*i);
		}

		return success;
	}

	enum RUN_STAGE
	{
		RS_START = 0,
		RS_QUERY,
		RS_PROMPT,
		RS_POST,
		RS_DONE,
	};

	void ToggleIdleHandler(bool active)
	{
		if (idleHandler == active) return;
		idleHandler = active;
		if (active) Connect(wxID_ANY, wxEVT_IDLE, wxIdleEventHandler(TattleApp::OnIdle));
		else        Disconnect(wxEVT_IDLE, wxIdleEventHandler(TattleApp::OnIdle));
	}

	/*
		This is a crude coroutine describing the workflow of a Tattle execution.
			It is called once on initialization, and again whenever a dialog completes.
			It executes until another dialog is created or the program completes.

	*/
	void Proceed()
	{
		// Make sure this doesn't carry over...
		pendingWindow = NULL;

		// Proceed
		switch (stage)
		{
		case RS_START:
			stage = RS_QUERY;
			PerformQuery();
			if (pendingWindow) break;
			[[fallthrough]];

		case RS_QUERY:
			stage = RS_PROMPT;
			PerformPrompt();
			if (pendingWindow) break;
			[[fallthrough]];

		case RS_PROMPT:
			if (stage <= RS_PROMPT) // Allow skipping via Halt()
			{
				stage = RS_POST;
				PerformPost();
			}
			if (pendingWindow) break;
			[[fallthrough]];

		case RS_POST:
			stage = RS_DONE;
			[[fallthrough]];

		case RS_DONE:
			if (prompt)
			{
				prompt->Destroy();
				prompt = NULL;
			}
			stage = RS_DONE;
			std::cout << "Done now..." << std::endl;
			printTopLevelWindows();
			break;
		}

		// Show any pending window
		if (pendingWindow)
		{
			anyWindows = true;
			pendingWindow->Show();
			pendingWindow->Raise();
			//pendingWindow = NULL;
		}

		// Register the idle handler to start the next task
		if (stage != RS_DONE) ToggleIdleHandler(true);
	}

	void ShowPrompt()
	{
		if (stage < RS_PROMPT)
		{
			stage = RS_QUERY;
			Proceed();
		}
		else if (prompt)
		{
			// Go back to the prompt step
			stage = RS_PROMPT;
			if (!prompt->IsEnabled()) prompt->Enable();
			if (!prompt->IsShown()) prompt->Show();
			prompt->Raise();
		}
		else
		{
			wxASSERT("Tattle workflow error: can't resume prompt");
		}
	}

	void InsertDialog(wxWindow *dialog)
	{
		pendingWindow = dialog;

		ToggleIdleHandler(true);
	}

	void DisposeDialog(wxWindow *dialog)
	{
		disposeWindow = dialog;

		ToggleIdleHandler(true);
	}

	void Halt()
	{
		stage = RS_DONE;
		Proceed();
	}

	void OnIdle(wxIdleEvent &event)
	{
		if (disposeWindow)
		{
			disposeWindow->Destroy();
			disposeWindow = NULL;
			return;
		}

		if (pendingWindow)
		{
			pendingWindow->Show();
			pendingWindow->Raise();
			pendingWindow = NULL;
		}

		// Unregister this handler
		ToggleIdleHandler(false);

		if (stage == RS_DONE)
		{
			std::cout << "Done and idle..." << std::endl;
			if (printTopLevelWindows())
				ToggleIdleHandler(true);
		}

		/*switch (stage)
		{
		case RS_START:
		case RS_QUERY:
			//Proceed();
			break;

		case RS_DONE:
			break;
		}*/
	}
	
	static bool printTopLevelWindows()
	{
		// Hmm
		wxWindowList::compatibility_iterator node = wxTopLevelWindows.GetFirst();
		if (node)
		{
			std::cout << "Top level windows: ";
			while (node)
			{
				wxTopLevelWindow* win = (wxTopLevelWindow*) node->GetData();
				
				std::cout
					// << "@" << win
					// << ":" << wxString(win->GetClassInfo()->GetClassName())
					<< "\"" << win->GetTitle() << "\"";
				
				node = node->GetNext();
				
				if (node) std::cout << ", ";
			}
			std::cout << std::endl;
		}
		
		return node != NULL;
	}

	// this one is called on application startup and is a good place for the app
	// initialization (doing it here and not in the ctor allows to have an error
	// return: if OnInit() returns false, the application terminates)
	virtual bool OnInit() wxOVERRIDE;

	virtual int OnExit() wxOVERRIDE;
	
	virtual int OnRun() wxOVERRIDE;

	void PerformQuery();
	void PerformPrompt();
	void PerformPost();

private:
RUN_STAGE stage;

wxWindow* pendingWindow, * disposeWindow;
Prompt* prompt;

bool idleHandler;
bool anyWindows;
};

bool TattleApp::OnInit()
{
	anyWindows = false;

	//cout << "Reading command line..." << endl;

	// call the base class initialization method, currently it only parses a
	// few common command-line options but it could be do more in the future
	if (!wxApp::OnInit())
		return false;

	tattleApp = this;
	stage = RS_START;
	pendingWindow = NULL;
	disposeWindow = NULL;
	prompt = NULL;

	bool badCmdLine = false;

	if (!report.url_post().isSet() && !report.url_query().isSet())
	{
		cout << "At least one URL must be set with the --url-* options." << endl;
		badCmdLine = true;
	}

	if (!report.params.size())
	{
		cout << "At least one argument (string/file/field) must be specified." << endl;
		badCmdLine = true;
	}

	if (badCmdLine)
	{
		cout << "  Execute tattle --help for more information." << endl;
		return false;
	}

	report_.readFiles();

	// Debug
	cout << "Successful init." << endl
		<< "  URL (post): " << report.url_post().full()
		<< "  Parameters:" << endl;
	for (Report::Parameters::const_iterator i = report.params.begin(); i != report.params.end(); ++i)
		cout << "    - " << i->name << "= `" << i->value() << "' Type#" << i->type << endl;

	Proceed();

	return true;
}

int TattleApp::OnExit()
{
	//cout << "Exiting..." << endl;
	return wxApp::OnExit();
}

void TattleApp::PerformQuery()
{
	/*
		Pre-query step, if applicable
	*/
	if (report.url_query().isSet())
	{
		Report::Reply reply = report.httpQuery(*this);

		if (reply.valid() && !uiConfig.silentQuery())
		{
			// Queue up the prompt window
			pendingWindow = Prompt::DisplayReply(reply);
		}
		else
		{
			// Flag for a connection warning.
			report_.connectionWarning = true;
		}
	}
	else if (!uiConfig.silentQuery())
	{
		// Probe the server to test the connection
		if (report.url_post().isSet() && !report.httpTest(*this, report.url_post()))
			report_.connectionWarning = true;
	}
}
void TattleApp::PerformPrompt()
{
	// Skip if no post or running silently
	if (report.url_post().isSet() && !uiConfig.silentPost())
	{
		auto identity = report_.identity();
		if (JsonFetch(persist.data, JsonPointer("/$show/" + identity.type + "/" + identity.id), 1) != 0)
		{
			// Set up the prompt window for display
			prompt = new Prompt(NULL, -1, report_);
			pendingWindow = prompt;
		}
		else
		{
			//TODO prevent submission
			Halt();
		}
	}
}
void TattleApp::PerformPost()
{
	// Could be query-only...
	if (!report.url_post().isSet()) return;
	
	Report::Reply reply = report.httpPost(prompt ? (wxEvtHandler&) *prompt : *this);

	if (reply.valid())
	{
		if (!reply.icon.length()) reply.icon = wxART_INFORMATION;
	}
	else
	{
		if (!reply.icon.length()) reply.icon = wxART_ERROR;

		// Flag for a connection warning.
		report_.connectionWarning = true;
	}

	// Queue up the info dialog
	if (!uiConfig.silentPost())
		pendingWindow = Prompt::DisplayReply(reply, prompt);
}

/*int TattleApp::OnIdle()
{
	//
}*/

int TattleApp::OnRun()
{
	if (!anyWindows)
	{
		ExitMainLoop();
		return 0;
	}
	
	return wxApp::OnRun();
}

void tattle::Tattle_Proceed()
{
	tattleApp->Proceed();
}
void tattle::Tattle_ShowPrompt()
{
	tattleApp->ShowPrompt();
}
void tattle::Tattle_InsertDialog(wxWindow *dialog)
{
	tattleApp->InsertDialog(dialog);
}
void tattle::Tattle_DisposeDialog(wxWindow *dialog)
{
	tattleApp->DisposeDialog(dialog);
}
void tattle::Tattle_Halt()
{
	tattleApp->Halt();
}

// the application icon (under Windows it is in resources and even
// though we could still include the XPM here it would be unused)
/*#ifndef wxHAS_IMAGES_IN_RESOURCES
#include "../sample.xpm"
#endif*/

// 
wxIMPLEMENT_APP(tattle::TattleApp);
