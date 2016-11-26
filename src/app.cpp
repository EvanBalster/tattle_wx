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
#define CMD_OPTION_STRINGS(SHORT, LONG, DESC)     { wxCMD_LINE_OPTION, SHORT, LONG, DESC, wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE }, 
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

		CMD_OPTION_STRING ("up", "url-post",      "<url>    HTTP URL for posting report.  Required.")
		CMD_OPTION_STRING ("uq", "url-query",     "<url>    HTTP URL for pre-query.")

		CMD_SWITCH        ("s",  "silent",        "Bypass prompt; upload without user input.")
		//CMD_SWITCH       ("t", "stay-on-top",     "Make the prompt GUI stay on top.")
		//CMD_SWITCH       ("w", "parent-window",   "Make the prompt GUI stay on top.")
		
		CMD_OPTION_STRINGS("c",  "config-file",   "<fname>              Config file with more arguments.")

		CMD_OPTION_STRINGS("a",  "arg",           "<parameter>=<value>  Argument string.")
		CMD_OPTION_STRINGS("aq", "arg-query",     "<parameter>=<value>  Argument string used in pre-query.")
		
		CMD_OPTION_STRINGS("ft", "file",          "<parameter>=<fname>  Argument text file.")
		CMD_OPTION_STRINGS("fb", "file-binary",   "<parameter>=<fname>  Argument file, binary.")
		
		CMD_OPTION_STRINGS("tb", "trunc-begin",   "<param>=<N>          Attach first N bytes of file.")
		CMD_OPTION_STRINGS("te", "trunc-end",     "<param>=<N>          Attach last N bytes of file.")
		CMD_OPTION_STRINGS("tn", "trunc-note",    "<param>=<text>       Line of text marking truncation.")

		CMD_OPTION_STRING ("pt", "title",         "<title>              Title of the prompt window.")
		CMD_OPTION_STRING ("pm", "message",       "<message>            A header message summarizing the prompt.")
		CMD_OPTION_STRING ("ps", "label-send",    "<text>               Text of the 'send' button.")
		CMD_OPTION_STRING ("pc", "label-cancel",  "<text>               Text of the 'cancel' button.")
		CMD_OPTION_STRING ("pv", "label-view",    "<text>               Text of the 'view data' button.")
		
		CMD_OPTION_STRINGS("i",  "field",         "<parameter>=<label>  Single-line field for user input.")
		CMD_OPTION_STRINGS("im", "field-multi",   "<parameter>=<label>  Multi-line field.")
		CMD_OPTION_STRINGS("id", "field-default", "<parameter>=<value>  Hint message for -uf field.")
		CMD_OPTION_STRINGS("ih", "field-hint",    "<parameter>=<hint>   Default value for a field.")

		//CMD_OPTION_STRINGS("ts", "tech-string",   "<label>:<value>      Technical info string.")
		//CMD_OPTION_STRINGS("tf", "tech-file",     "<label>:<fname>      Technical info as a linked file.")
		CMD_SWITCH        ("v" , "view-data",     "                     Enable 'view data' dialog.")
		CMD_OPTION_STRINGS("vd", "view-dir",      "<path>               Folder listed in 'view data' dialog.")

		{wxCMD_LINE_NONE}
	};
}

using namespace tattle;

// Main application object.
class tattle::TattleApp : public wxApp
{
public:
	// override base class virtuals
	// ----------------------------

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
		
		//cout << ">>>>" << argName << " " << arg.GetStrVal() << endl;;

		do
		{
			if (c0 == 'u')
			{
				/*
					URLs
				*/
				if (c1 == 'p')
				{
					if (!report.postURL.set(arg.GetStrVal())) err = CMD_ERR_BAD_URL;
				}
				else if (c1 == 'q')
				{
					if (!report.queryURL.set(arg.GetStrVal())) err = CMD_ERR_BAD_URL;
				}
				else err = CMD_ERR_UNKNOWN;
			}
			else if (c0 == 's')
			{
				report.silent = true;
			}
			else if (c0 == 'c')
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
				else err = CMD_ERR_FAILED_TO_OPEN_FILE;
			}
			else if (c0 == 'a')
			{
				// Argument string
				wxString name, value;
				if (!ParsePair(arg.GetStrVal(), name, value)) { err = CMD_ERR_BAD_PAIR; break; }

				//Parameter must not exist.
				if (report.findParam(name) != NULL) { err = CMD_ERR_PARAM_REDECLARED; break; }

				Report::Parameter param;
				param.type = PARAM_STRING;
				param.name = name;
				param.value = value;
				param.preQuery = (c1 == 'q');
				report.params.push_back(param);
			}
			else if (c0 == 'f')
			{
				// Argument string
				wxString name, value;
				if (!ParsePair(arg.GetStrVal(), name, value)) { err = CMD_ERR_BAD_PAIR; break; }

				//Parameter must not exist.
				if (report.findParam(name) != NULL) { err = CMD_ERR_PARAM_REDECLARED; break; }
					
				Report::Parameter param;
				if      (c1 == 't') {param.type = PARAM_FILE_TEXT; param.fname = value;}
				else if (c1 == 'b') {param.type = PARAM_FILE_BIN;  param.fname = value;}
				else err = CMD_ERR_UNKNOWN;
				param.name = name;
				param.fname = value;
				
				// Read the file...
				wxFile file(value);
				if (file.IsOpened())
				{
					if (param.type == PARAM_FILE_TEXT)
					{
						// Content info
						param.contentInfo = "Content-Type: text/plain";
					}
					else
					{
						// Content info
						param.contentInfo = "Content-Type: application/octet-stream"
							"\r\n"
							"Content-Transfer-Encoding: Base64";
					}
				}
				else err = CMD_ERR_FAILED_TO_OPEN_FILE;
				
				report.params.push_back(param);
			}
			else if (c0 == 't')
			{
				// Argument string
				wxString name, value;
				if (!ParsePair(arg.GetStrVal(), name, value)) { err = CMD_ERR_BAD_PAIR; break; }
				
				//Parameter must exist and must be a file
				Report::Parameter *param = report.findParam(name);
				if (param == NULL)
					{ err = CMD_ERR_PARAM_MISSING; break; }
				if (param->type != PARAM_FILE_BIN && param->type != PARAM_FILE_TEXT)
					{ err = CMD_ERR_PARAM_NOT_APPLICABLE; break; }
				
				if (c1 == 'b' || c1 == 'e')
				{
					long bytes = 0;
					if (value.ToLong(&bytes) && bytes >= 0)
					{
						((c1 == 'b') ? param->trimBegin : param->trimEnd) = unsigned(bytes);
					}
					else err = CMD_ERR_BAD_SIZE;
				}
				else if (c1 == 'n')
				{
					param->trimNote = value;
				}
				else err = CMD_ERR_UNKNOWN;
			}
			else if (c0 == 'p')
			{
				//Prompt stuff
				if      (c1 == 't') report.promptTitle   = arg.GetStrVal();
				else if (c1 == 'm') report.promptMessage = arg.GetStrVal();
				else if (c1 == 's') report.labelSend     = arg.GetStrVal();
				else if (c1 == 'c') report.labelCancel   = arg.GetStrVal();
				else if (c1 == 'v') report.labelView     = arg.GetStrVal();
				else err = CMD_ERR_UNKNOWN;
			}
			else if (c0 == 'i')
			{
				if (c1 == '\0' || c1 == 'm')
				{
					wxString name, label;
					if (!ParsePair(arg.GetStrVal(), name, label)) { err = CMD_ERR_BAD_PAIR; break; }

					//Parameter must not exist.
					if (report.findParam(name) != NULL) { err = CMD_ERR_PARAM_REDECLARED; break; }

					Report::Parameter param;
					param.name = name;
					param.label = label;
					if (c1 == '\0')      param.type = PARAM_FIELD;
					else if (c1 == 'm') param.type = PARAM_FIELD_MULTI;
					report.params.push_back(param);
				}
				else if (c1 == 'h' || c1 == 'd')
				{
					wxString name, value;
					if (!ParsePair(arg.GetStrVal(), name, value)) { err = CMD_ERR_BAD_PAIR; break; }

					// Value must exist
					Report::Parameter *param = report.findParam(name);
					if (param == NULL) { err = CMD_ERR_PARAM_MISSING; break; }

					if (c1 == 'h')
					{
						//Add a hint.
						if (param->type != PARAM_FIELD) { err = CMD_ERR_PARAM_NOT_APPLICABLE; }
						param->hint = value;
					}
					else if (c1 == 'd')
					{
						//Add a hint.
						if (param->type != PARAM_FIELD && param->type != PARAM_FIELD_MULTI) { err = CMD_ERR_PARAM_NOT_APPLICABLE; }
						param->value = value;
					}
				}
				else err = CMD_ERR_UNKNOWN;
			}
			else if (c0 == 'v')
			{
				if (c1 == '\0')
				{
					report.viewEnabled = true;
				}
				if (c1 == 'd')
				{
					report.viewPath = arg.GetStrVal();
				}
				else err = CMD_ERR_UNKNOWN;
			}
			else err = CMD_ERR_UNKNOWN;
		}
		while (0);

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

	// this one is called on application startup and is a good place for the app
	// initialization (doing it here and not in the ctor allows to have an error
	// return: if OnInit() returns false, the application terminates)
	virtual bool OnInit() wxOVERRIDE;
	
	virtual int OnRun() wxOVERRIDE;

private:
	Report report;
	
	bool earlyFinish;
};

bool TattleApp::OnInit()
{
	earlyFinish = false;
	
	//cout << "Reading command line..." << endl;

	// call the base class initialization method, currently it only parses a
	// few common command-line options but it could be do more in the future
	if (!wxApp::OnInit())
		return false;
		
	bool badCmdLine = false;
		
	if (!report.postURL.isSet() && !report.queryURL.isSet())
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
	
	report.readFiles();

	// Debug
	cout << "Successful init." << endl
		<< "  URL: http://" << report.postURL.host << report.postURL.path << endl
		<< "  Parameters:" << endl;
	for (Report::Parameters::iterator i = report.params.begin(); i != report.params.end(); ++i)
		cout << "    - " << i->name << "= `" << i->value << "' Type#" << i->type << endl;
	
	/*
		Pre-query step, if applicable
	*/
	if (report.queryURL.isSet())
	{
		Report::Reply reply = report.httpQuery();
		
		if (reply.valid())
		{
			bool usedLink = Prompt::DisplayReply(reply, 0);
			
			if (reply.command == Report::SC_STOP ||
				(reply.command == Report::SC_STOP_ON_LINK && usedLink))
			{
				earlyFinish = true;
				return true;
			}
		}
		else
		{
			report.connectionWarning = true;
		}
	}
	else if (!report.silent)
	{
		if (!report.httpTest(report.postURL))
			report.connectionWarning = true;
	}
	
	if (!report.postURL.isSet())
	{
		// Query only, apparently
	}
	if (report.silent)
	{
		// Fire a POST request and hope for the best
		report.httpPost();
		
		earlyFinish = true;
		return true;
	}
	else
	{
		// create the main dialog
		Prompt *prompt = new Prompt(NULL, -1, report);
		
		// Show the dialog, non-modal
		prompt->Show(true);
		
		// Continue execution
		return true;
	}
}

int TattleApp::OnRun()
{
	if (earlyFinish)
	{
		ExitMainLoop();
		return 0;
	}
	
	return wxApp::OnRun();
}

// the application icon (under Windows it is in resources and even
// though we could still include the XPM here it would be unused)
/*#ifndef wxHAS_IMAGES_IN_RESOURCES
#include "../sample.xpm"
#endif*/

// 
wxIMPLEMENT_APP(tattle::TattleApp);
