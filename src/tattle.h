//
//  tattle.h
//  tattle
//
//  Created by Evan Balster on 11/12/16.
//

#ifndef tattle_h
#define tattle_h

#include <string>
#include <iostream>
#include <list>

#if FORCE_TR1_TYPE_TRAITS
    // Hack to deal with STL weirdness on OS X
	#include <wx/setup.h>
    #include <wx/defs.h>
	#ifdef HAVE_TYPE_TRAITS
		#undef HAVE_TYPE_TRAITS
	#endif
	#ifndef HAVE_TR1_TYPE_TRAITS
		#define HAVE_TR1_TYPE_TRAITS
	#endif
#endif

#include <wx/string.h>
#include <wx/file.h>
#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/msgdlg.h>
#include <wx/artprov.h>
#include <wx/hyperlink.h>
#include <wx/progdlg.h>

#include <wx/protocol/http.h>

namespace tattle
{
	using std::cout;
	using std::endl;

	/*
		Workflow control calls
	*/
	void Tattle_Proceed();
	void Tattle_ShowPrompt();
	void Tattle_InsertDialog(wxWindow *dialog);
	void Tattle_DisposeDialog(wxWindow *dialog);
	void Tattle_Halt();
	
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
            Parameter();
            
            PARAM_TYPE  type;
            wxString    name;
			
			// Include in pre-query
			bool        preQuery;
			
            // Filename if applicable
            wxString    fname;
            
            // String value for arguments & prompts
            wxString    value;
			
			// File contents
			wxMemoryBuffer fileContents;
			
			// Fields for user prompts only
            wxString    label, hint;
			
			// File truncation parameters
			unsigned    trimBegin, trimEnd;
			wxString    trimNote;
            
            // MIME content type and encoding information, if applicable.
            //   If non-empty, will be added to multipart POST request.
            std::string contentInfo;
        };
        
        typedef std::list<Parameter> Parameters;
		
		/*
			Represents a parsed HTTP URL.
				base is an IP or hostname string, EG. uploads.example.com
				path is the path following the hostname, EG. /error/report.php
		*/
		struct ParsedURL
		{
			wxString host, path;
			unsigned long port;
			
			
			ParsedURL() {port=80;}
			
			// Set to a URL.  Only host, path and port will be used.
			bool set(wxString fullURL);
			
			bool isSet() const    {return host.Length() != 0;}
		};
		
		/*
			When performing a pre-query, the
		*/
		enum SERVER_COMMAND
		{
			SC_NONE         = 0, // No advice; proceed as usual
			SC_STOP         = 1, // Stop after displaying response
			SC_PROMPT       = 2, // Prompt after displaying response
			SC_STOP_ON_LINK = 3, // Halt only if link is followed
		};
		
		/*
			Represents the result of an HTTP query
		*/
		struct Reply
		{
			Reply();
			
			// HTTP status
			bool            connected;  // Whether we connected to the server
			int             statusCode; // HTTP status, or 0 if not connected
			wxProtocolError error;
			
			wxString       raw;
			wxString       title, message, link;
			SERVER_COMMAND command;
			wxArtID        icon;
			
			bool ok()       const;
			bool valid()    const;
			bool sentLink() const;
			
			
			
			
			// Fill in string fields from HTTP request / stream
			void connect(wxHTTP &http, const ParsedURL &url);
			void pull(wxHTTP &http, const ParsedURL &url, wxString query = wxT(""));
			
			// Assigns title, message and link based on raw reply text
			void parseRaw(const ParsedURL &url);
		};
		
		
	public:
		Report();
	
		Parameter       *findParam(const wxString &name);
		const Parameter *findParam(const wxString &name) const;
		
		// Read file contents into fileContents buffers
		void readFiles();
		
		
		// Query the server using the query address.
		Reply httpQuery(wxWindow *parent = NULL) const;
		
		// Perform HTTP post, returning whether successful.
		Reply httpPost(wxWindow *parent = NULL) const;
		
		// Test connectivity by making a test connection (but no actual HTTP query)
		bool  httpTest(const ParsedURL &url) const;
		
        
        // Encode HTTP query and post request
		wxString preQueryString()                           const;
        void     encodePost(wxHTTP &request, bool preQuery) const;
        
    public: // members
		void httpAction(wxHTTP &http, const ParsedURL &url, Reply &reply, wxProgressDialog *dlg, bool isQuery) const;

		ParsedURL postURL, queryURL;
        
		wxString viewPath;
		
		bool connectionWarning;
        
        Parameters params;
    };

	struct UIConfig
	{
		UIConfig();

		wxString promptTitle;
		wxString promptMessage;
		wxString promptTechnical;

		wxString labelSend, labelCancel, labelView;

		bool viewEnabled;
		bool stayOnTop;
		bool showProgress;
		bool silent;

		unsigned marginSm, marginMd, marginLg; // defaults to 5, 8, 10

		wxArtID defaultIcon; // wxART_ERROR


		wxArtID GetIconID(wxString tattleName) const;


		int style() const
		{
			return (stayOnTop ? wxSTAY_ON_TOP : 0);
		}
	};

	/*
		Globally shared report state and UI configuration.
			Only one report is made per invocation of Tattle.
	*/
	extern const Report   &report;
	extern const UIConfig &uiConfig;
    
    /*
		A prompt window which allows the user to enter data and submit the report.
    */
    class Prompt : public wxDialog
    {
    public:
        enum EventTypes
        {
            Ev_Exit    = wxID_EXIT,
            Ev_Cancel  = wxID_CANCEL,
            Ev_Submit  = wxID_FORWARD,
            Ev_Details = wxID_INFO,
        };
        
        Prompt(wxWindow * parent, wxWindowID id, Report &report);
		~Prompt() wxOVERRIDE;
		
		/*
			Create a dialog box describing a server's reply.
				Returns true if the user followed a link.
		*/
		static wxWindow *DisplayReply(const Report::Reply &reply, wxWindow *parent = NULL);
        
    private:
        struct Field
        {
            Report::Parameter *param;
            wxTextCtrl        *control;
        };
        typedef std::vector<Field> Fields;
        
        void UpdateReportFromFields();
        
        void OnSubmit (wxCommandEvent & event);
        void OnCancel (wxCommandEvent & event);
        void OnDetails(wxCommandEvent & event);
        void OnClose  (wxCloseEvent   & event);

		void OnShow   (wxShowEvent & event);
        
        Report &report;
        Fields  fields;
		
		wxFont fontTechnical;
        
        wxDECLARE_EVENT_TABLE();
    };
	
	/*
		This dialog allows a user to view a report's contents
	*/
	class ViewReport : public wxDialog
    {
    public:
        enum EventTypes
        {
            Ev_Exit     = wxID_EXIT,
            Ev_Done     = wxID_OK,
			Ev_OpenFile = wxID_FILE,
			Ev_OpenDir  = wxID_OPEN,
        };
        
        ViewReport(wxWindow * parent, wxWindowID id);
		virtual ~ViewReport();
		
		static bool Exists();
        
    private:
        void OnDone    (wxCommandEvent & event);
		void OnOpenFile(wxCommandEvent & event);
		void OnOpenDir (wxCommandEvent & event);
        void OnClose   (wxCloseEvent   & event);
		
		wxFont fontTechnical;
        
        wxDECLARE_EVENT_TABLE();
    };

	/*
		Non-modal Dialogs integrated with Tattle's workflow.
			May be "OK" or "Open Link / Cancel" dialogs.
	*/
	class InfoDialog : public wxDialog
	{
	public:
		/*
			Create a Tattle dialog.
				- Specified title and message, with "OK" button
				- Optionally specify a link
				- Uses styling from uiConfig
				- Calls Tattle_Proceed / Tattle_Halt when done, depending on server command:
					- NONE:         proceed as usual
					- PROMPT:       return to prompt if applicable, else proceed
					- STOP:         always halt
					- STOP_ON_LINK: halt when following link, else proceed
		*/
		InfoDialog(wxWindow *parent,
			wxString title,
			wxString message,
			wxString link = wxString(),
			Report::SERVER_COMMAND command = Report::SC_NONE,
			wxArtID iconArtID = "");

	protected:
		void Done();

		void OpenLink();

		void OnOk(wxCommandEvent &evt);
		void OnLink(wxHyperlinkEvent &evt);
		void OnOpen(wxCommandEvent &evt);
		void OnCancel(wxCommandEvent &evt);

		void OnClose(wxCloseEvent &evt);

		wxDECLARE_EVENT_TABLE();

	protected:
		Report::SERVER_COMMAND command;

		wxString link;
		int      styleBase;

		bool     didAction;
		bool     overrideCommand;
	};
	
	// Utility functions
	wxString GetTagContents(const wxString &reply, const wxString &tagName);
	
	class TattleApp;
}

#endif /* tattle_h */
