/*
	"tattle" utility for reporting errors, warnings and feedback.

	See macro table below or run with --help for a listing of command-line options

	Depends on wxWidgets 3.  Uses wxCore, wxBase and wxNet libraries.
	Available under the wxWidgets license, or the zlib license (see bottom)

	EXAMPLE ARGS:
		-u "http://example.com/report.php" -pt "Error Report" -pi "imitone crashed!  Tell us what happened and we'll investigate." -pm "user.desc:Use this space to tell us what happened:" -pf "user.email:Your E-mail (optional):" -ph "user.email:you@example.com"
*/

#include <iostream>
#include <list>

#include <wx/app.h>

//Dialog
#include <wx/dialog.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/statline.h>
#include <wx/textctrl.h>
#include <wx/sizer.h>

//Command line
#include <wx/cmdline.h>

//File reading
#include <wx/file.h>

//Uploading
#include <wx/sstream.h>
#include <wx/protocol/http.h>
#include <wx/base64.h>

//Debug messages
#include <wx/msgdlg.h>

#define CMD_HELP(SHORT, LONG, DESC)              { wxCMD_LINE_SWITCH, SHORT, LONG, DESC, wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
#define CMD_SWITCH(SHORT, LONG, DESC)            { wxCMD_LINE_SWITCH, SHORT, LONG, DESC, wxCMD_LINE_VAL_NONE },
#define CMD_OPTION_STRING(SHORT, LONG, DESC)     { wxCMD_LINE_OPTION, SHORT, LONG, DESC, wxCMD_LINE_VAL_STRING }, 
#define CMD_OPTION_STRINGS(SHORT, LONG, DESC)     { wxCMD_LINE_OPTION, SHORT, LONG, DESC, wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE }, 
#define CMD_REQUIRE_STRING(SHORT, LONG, DESC)    { wxCMD_LINE_OPTION, SHORT, LONG, DESC, wxCMD_LINE_VAL_STRING, wxCMD_LINE_OPTION_MANDATORY },
#define CMD_OPTION_INT(SHORT, LONG, DESC)        { wxCMD_LINE_OPTION, SHORT, LONG, DESC, wxCMD_LINE_VAL_NUMBER },

namespace tattle
{
	using std::cout;
	using std::endl;

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

		CMD_OPTION_STRINGS("as", "string",        "<parameter>:<value>  Argument string.")
		CMD_OPTION_STRINGS("ft", "file",          "<parameter>:<fname>  Argument text file.")
		CMD_OPTION_STRINGS("fb", "file-binary",   "<parameter>:<fname>  Argument file, binary.")

		CMD_OPTION_STRING ("pt", "title",         "<title>              Title of the prompt window.")
		CMD_OPTION_STRING ("pi", "message",       "<message>            A header message summarizing the prompt.")
		CMD_OPTION_STRINGS("pf", "field",         "<parameter>:<label>  Single-line field for a user argument.")
		CMD_OPTION_STRINGS("pm", "field-box",     "<parameter>:<label>  Multi-line field.")
		CMD_OPTION_STRINGS("pd", "field-default", "<parameter>:<hint>   Hint message for -pf field.")
		CMD_OPTION_STRINGS("ph", "field-hint",    "<parameter>:<hint>   Default value for a field.")

		//CMD_OPTION_STRINGS("ts", "tech-string",   "<label>:<value>      Technical info string.")
		//CMD_OPTION_STRINGS("tf", "tech-file",     "<label>:<fname>      Technical info as a linked file.")
		CMD_OPTION_STRINGS("td", "tech-dir",      "<label>:<path>       Technical info as a linked folder.")

		{wxCMD_LINE_NONE}
	};

	enum PARAM_TYPE
	{
		PARAM_NONE = 0,
		PARAM_STRING,
		PARAM_FILE_BIN,
		PARAM_FILE_TEXT,
		PARAM_FIELD,
		PARAM_FIELD_MULTI,
	};

	enum DETAIL_TYPE
	{
		DETAIL_NONE = 0,
		DETAIL_STRING = 1,
		DETAIL_TEXTFILE = 1,
		DETAIL_FILE = 2,
		DETAIL_DIR = 3,
	};

	/*
		Data structure defining the report.
	*/
	struct Report
	{
	public: //types

		struct Parameter
		{
			Parameter() : type(PARAM_NONE) {}
		
			PARAM_TYPE type;
			wxString   name;
			
			// Filename if applicable
			wxString   fname;
			
			// String value or file content
			wxString   value;
			
			// MIME content type and encoding information, if applicable.
			//   If non-empty, will be added to multipart POST request.
			wxString   contentInfo;

			// Fields for user prompts only
			wxString   label, hint;
		};

		typedef std::list<Parameter> Parameters;

		struct Detail
		{
			DETAIL_TYPE type;
			wxString    label;
			wxString    value;
		};

		typedef std::list<Detail> Details;

		Parameter *findParam(const wxString &name)
		{
			for (Parameters::iterator i = params.begin(); i != params.end(); ++i) if (i->name == name) return &*i;
			return NULL;
		}
		
		// Encode HTTP post request
		void encodePost(wxHTTP &request) const;

	public: // members

		wxString uploadUrl;

		wxString promptTitle;
		wxString promptMessage;

		bool silent;

		Parameters params;
		Details    details;

	public:

		Report() { silent = false; }
	};
	
	void Report::encodePost(wxHTTP &http) const
	{
		// Boundary, should be ASCII
		wxString boundary = wxT("tattle-boundary-029781054--");
		
		wxString postBuffer;
		
		wxString boundaryFull = wxT("\r\n--") + boundary + wxT("\r\n");
		
		for (Parameters::const_iterator i = params.begin(); true; ++i)
		{
			// Boundary, before and after all items.
			postBuffer += boundaryFull;
			if (i == params.end()) break;
		
			// Content disposition, name, filename
			postBuffer += wxT("Content-Disposition: form-data; name=\"") + i->name + wxT("\"");
			if (i->fname.length()) postBuffer += " filename=\"" + i->fname + wxT("\"");
			postBuffer += wxT("\r\n");
			
			// Content type
			if (i->contentInfo.length()) postBuffer += i->contentInfo + wxT("\r\n");
			
			// Empty line...
			postBuffer += wxT("\r\n");
			
			// Content...
			postBuffer += i->value;
		}
		
		wxScopedCharBuffer utf8 = postBuffer.ToUTF8();
		wxMemoryBuffer encodeBuff;
		encodeBuff.AppendData((void*) utf8.data(), utf8.length());
		
		http.SetPostBuffer(wxT("multipart/form-data; boundary=\"") + boundary + ("\""), encodeBuff);
		
		cout << "HTTP Post Buffer:" << endl << postBuffer.ToUTF8() << endl;
	}

	// Main application object.
	class TattleApp : public wxApp
	{
	public:
		// override base class virtuals
		// ----------------------------

		static bool ParsePair(const wxString &str, wxString &first, wxString &second)
		{
			wxString::size_type p = str.find_first_of(':');
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
					if (c1 == 's') param.type = PARAM_STRING;
					else err = CMD_ERR_UNKNOWN;
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
							// Read the binary file and base64 encode it.
							wxMemoryBuffer contentBuffer;
							wxFileOffset contentLength = file.Length();
							size_t contentConsumed = file.Read(
								contentBuffer.GetAppendBuf(contentLength), contentLength);
							contentBuffer.UngetAppendBuf(contentConsumed);
							param.value = wxBase64Encode(contentBuffer);
							
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

	// the application icon (under Windows it is in resources and even
	// though we could still include the XPM here it would be unused)
/*#ifndef wxHAS_IMAGES_IN_RESOURCES
#include "../sample.xpm"
#endif*/

	class Prompt : public wxDialog
	{
	public:
		enum EventTypes
		{
			Ev_Exit = wxID_EXIT,
			Ev_Cancel = wxID_CANCEL,
			Ev_Submit = wxID_OK,
			Ev_Details = wxID_VIEW_DETAILS,
		};

		Prompt(wxWindow * parent, wxWindowID id, Report &report);

	private:
		struct Field
		{
			Report::Parameter *param;
			wxTextCtrl        *control;
		};
		typedef std::vector<Field> Fields;
	
		void UpdateReportFromFields();

		void OnSubmit(wxCommandEvent & event);
		void OnCancel(wxCommandEvent & event);
		void OnDetails(wxCommandEvent & event);
		void OnClose(wxCloseEvent   & event);

		Report &report;
		Fields  fields;

		wxDECLARE_EVENT_TABLE();
	};

	wxBEGIN_EVENT_TABLE(Prompt, wxDialog)
		EVT_BUTTON(Prompt::Ev_Submit,  Prompt::OnSubmit)
		EVT_BUTTON(Prompt::Ev_Cancel,  Prompt::OnCancel)
		EVT_BUTTON(Prompt::Ev_Details, Prompt::OnDetails)
		EVT_CLOSE (Prompt::OnClose)
	wxEND_EVENT_TABLE()
	
	void Prompt::OnDetails(wxCommandEvent & event)
	{
		wxMessageBox(_("User requested details."));
	}
	
	void Prompt::OnSubmit(wxCommandEvent & event)
	{
		UpdateReportFromFields();
	
		wxHTTP post;
		report.encodePost(post);
		
		post.SetTimeout(10);
		
		//wxString 
		
		if (!post.Connect(wxT("imitone.com")))
		{
			wxMessageBox(_("Failed to reach the website for the error report.  Are you connected to the internet?"));
			post.Close();
			return;
		}
		
		wxInputStream *httpStream = post.GetInputStream(_T("/mothership/error_test.php"));
		
		if (post.GetError() == wxPROTO_NOERR)
		{
			wxString reply;
			wxStringOutputStream out_stream(&reply);
			httpStream->Read(out_stream);
		 
			wxMessageBox(wxT("WEBSITE REPLY:\r\n") + reply);
			// wxLogVerbose( wxString(_T(" returned document length: ")) << res.Length() );
		}
		else
		{
			wxMessageBox(wxString::Format(wxT("Failed to upload the report: error %i\r\n%s"),post.GetError()));
		}
		 
		wxDELETE(httpStream);
		post.Close();
		
		Close(true);
	}
	
	void Prompt::OnCancel(wxCommandEvent & event)
	{
		//wxMessageBox(_("User canceled the report."));
		Close(true);
	}
	
	void Prompt::UpdateReportFromFields()
	{
		for (Fields::iterator i = fields.begin(); i != fields.end(); ++i)
		{
			i->param->value = i->control->GetValue();
		}
	}

	// Dialog
	Prompt::Prompt(wxWindow * parent, wxWindowID id, Report &_report)
		: wxDialog(parent, id, _report.promptTitle,
			wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
		report(_report)
	{
		// Error display ?
		//wxTextCtrl *displayError 

		wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);

		if (report.promptMessage.length())
		{
			sizerTop->Add(
				new wxStaticText(this, -1, report.promptMessage,
					wxDefaultPosition, wxDefaultSize, wxCENTER),
				0, wxALIGN_LEFT | wxALL, 5);

			// Horizontal rule
			sizerTop->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, 5);
		}

		wxFlexGridSizer *sizerField = NULL;

		for (Report::Parameters::iterator i = report.params.begin(); i != report.params.end(); ++i)
			switch (i->type)
		{
		case PARAM_FIELD:
			{
				wxStaticText *label = new wxStaticText(this, -1, i->label, wxDefaultPosition, wxDefaultSize, wxLEFT);

				wxTextCtrl *field = new wxTextCtrl(this, -1, i->value,
					wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, i->name);
					
				Field fieldEntry = {&*i, field}; fields.push_back(fieldEntry);

				if (i->hint.length()) field->SetHint(i->hint);

				if (!sizerField)
				{
					// Start a new flex grid for this run of one-liner fields
					sizerField = new wxFlexGridSizer(2);
					sizerField->AddGrowableCol(1, 1);
					sizerTop->Add(sizerField, 0, wxEXPAND | wxALL, 0);
				}

				sizerField->Add(label, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
				sizerField->Add(field, 0, wxEXPAND | wxALL, 5);
			}
			break;

		case PARAM_FIELD_MULTI:
			{
				if (sizerField) sizerField = NULL;

				wxStaticText *label = new wxStaticText(this, -1, i->label, wxDefaultPosition, wxDefaultSize, wxLEFT);

				wxTextCtrl *field = new wxTextCtrl(this, -1, i->value,
					wxDefaultPosition, wxSize(400, 200), wxTE_MULTILINE, wxDefaultValidator, i->name);
					
				Field fieldEntry = {&*i, field}; fields.push_back(fieldEntry);

				sizerTop->Add(label, 0, wxEXPAND | wxALL, 5);
				sizerTop->Add(field, 0, wxEXPAND | wxALL, 5);
			}
			
		default:
			// Not fields
			break;
		}

		{
			wxBoxSizer *actionRow = new wxBoxSizer(wxHORIZONTAL);

			wxButton
				*butSubmit = new wxButton(this, wxID_OK, _("Send Error Report")),
				*butCancel = new wxButton(this, wxID_CANCEL, _("Don't Send"));
				//*butDetails = new wxButton(this, wxID_VIEW_DETAILS, _("View Details..."));

			actionRow->Add(butSubmit, 1, wxEXPAND | wxALL, 5);
			actionRow->Add(butCancel, 0, wxALL, 5);
			//actionRow->Add(butDetails, 0, wxALL, 5);

			butSubmit->SetDefault();

			sizerTop->Add(actionRow, 0, wxALIGN_CENTER | wxALL);
		}

		// Apply the sizing scheme
		SetSizerAndFit(sizerTop);
	}

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
	
	void Prompt::OnClose(wxCloseEvent &event)
	{
		// TODO consider veto
		Destroy();
	}
}


// 
wxIMPLEMENT_APP(tattle::TattleApp);
