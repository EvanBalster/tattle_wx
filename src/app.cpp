//
//  app.cpp
//  tattle
//
//  Created by Evan Balster on 11/12/16.
//  Copyright © 2016 imitone. All rights reserved.
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

		CMD_OPTION_STRING ("u", "url",            "Target URL for reporting.  Required.")

		CMD_SWITCH        ("s", "silent",         "Upload the report without prompting the user.")
		//CMD_SWITCH       ("t", "stay-on-top",     "Make the prompt GUI stay on top.")
		//CMD_SWITCH       ("w", "parent-window",   "Make the prompt GUI stay on top.")
		
		CMD_OPTION_STRINGS("c",  "config-file",   "<fname>              Config file with more options, 1/line.")

		CMD_OPTION_STRINGS("a",  "arg",           "<parameter>=<value>  Argument string.")
		CMD_OPTION_STRINGS("ft", "file",          "<parameter>=<fname>  Argument text file.")
		CMD_OPTION_STRINGS("fb", "file-binary",   "<parameter>=<fname>  Argument file, binary.")

		CMD_OPTION_STRING ("pt", "title",         "<title>              Title of the prompt window.")
		CMD_OPTION_STRING ("pi", "message",       "<message>            A header message summarizing the prompt.")
		CMD_OPTION_STRINGS("pf", "field",         "<parameter>=<label>  Single-line field for a user argument.")
		CMD_OPTION_STRINGS("pm", "field-multi",   "<parameter>=<label>  Multi-line field.")
		CMD_OPTION_STRINGS("pd", "field-default", "<parameter>=<value>  Hint message for -pf field.")
		CMD_OPTION_STRINGS("ph", "field-hint",    "<parameter>=<hint>   Default value for a field.")

		//CMD_OPTION_STRINGS("ts", "tech-string",   "<label>:<value>      Technical info string.")
		//CMD_OPTION_STRINGS("tf", "tech-file",     "<label>:<fname>      Technical info as a linked file.")
		CMD_OPTION_STRINGS("cd", "content-dir",   "<label>=<path>       Technical info as a linked folder.")

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
		if (p == wxString::npos || p == 0 || p == str.length()-1) return false;

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
	};

	virtual void OnInitCmdLine(wxCmdLineParser& parser) wxOVERRIDE
	{
		parser.SetDesc(g_cmdLineDesc);
	}
	
	bool ExecCmdLineArg(const wxCmdLineArg &arg)
	{
		bool success = true;
		
		wxString argName = arg.GetShortName();
		char c0 = char(argName[0]), c1 = ((argName.length() > 1) ? char(argName[1]) : '\0');
		
		CMD_LINE_ERR err = CMD_ERR_NONE;
		
		//cout << ">>>>" << argName << " " << arg.GetStrVal() << endl;;

		do
		{
			if (c0 == 'u')
			{
				report.uploadUrl = arg.GetStrVal();
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
						// Read into value string
						file.ReadAll(&param.value);
						
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
			else if (c0 == 'p')
			{
				//Prompt stuff
				if (c1 == 't')
				{
					report.promptTitle = arg.GetStrVal();
				}
				else if (c1 == 'i')
				{
					report.promptMessage = arg.GetStrVal();
				}
				else if (c1 == 'f' || c1 == 'm')
				{
					wxString name, label;
					if (!ParsePair(arg.GetStrVal(), name, label)) { err = CMD_ERR_BAD_PAIR; break; }

					//Parameter must not exist.
					if (report.findParam(name) != NULL) { err = CMD_ERR_PARAM_REDECLARED; break; }

					Report::Parameter param;
					param.name = name;
					param.label = label;
					if (c1 == 'f')      param.type = PARAM_FIELD;
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
			else if (c0 == 't')
			{
				if (c1 == 'd')
				{
					// Argument string
					wxString label, path;
					if (!ParsePair(arg.GetStrVal(), label, path)) { err = CMD_ERR_BAD_PAIR; break; }

					Report::Detail detail;
					detail.type = DETAIL_DIR;
					detail.label = label;
					detail.value = path;
					report.details.push_back(detail);
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
				<< "' -- should be \"<first_part>:<second_part>\" (without pointy brackets)" << endl;
			//success = false;
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

private:
	Report report;
};

bool TattleApp::OnInit()
{
	report.promptTitle = "Tattle Report";
	
	//cout << "Reading command line..." << endl;

	// call the base class initialization method, currently it only parses a
	// few common command-line options but it could be do more in the future
	if (!wxApp::OnInit())
		return false;
		
	bool badCmdLine = false;
		
	if (!report.uploadUrl.length())
	{
		cout << "An upload URL must be specified with the -u or --url option." << endl;
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

	// Debug
	cout << "Successful init." << endl
		<< "  URL: " << report.uploadUrl << endl
		<< "  Parameters:" << endl;
	for (Report::Parameters::iterator i = report.params.begin(); i != report.params.end(); ++i)
		cout << "    - " << i->name << "= `" << i->value << "' Type#" << i->type << endl;

	// create the main dialog
	Prompt *prompt = new Prompt(NULL, -1, report);

	// Show the dialog, non-modal
	prompt->Show(true);

	// Continue execution
	return true;
}

// the application icon (under Windows it is in resources and even
// though we could still include the XPM here it would be unused)
/*#ifndef wxHAS_IMAGES_IN_RESOURCES
#include "../sample.xpm"
#endif*/

// 
wxIMPLEMENT_APP(tattle::TattleApp);
