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

#include <wx/protocol/http.h>

namespace tattle
{
	using std::cout;
	using std::endl;
	
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
			
			wxString    raw;
			wxString    title, message, link;
			SERVER_COMMAND command;
			
			bool ok()       const;
			bool valid()    const;
			bool sentLink() const;
			
			
			
			
			// Fill in string fields from HTTP request / stream
			void pull(wxHTTP &http, const ParsedURL &url, wxString query = wxT(""));
			
			// Assigns title, message and link based on raw reply text
			void parseRaw(const ParsedURL &url);
		};
		
		
	public:
		Report();
	
        Parameter *findParam(const wxString &name);
		
		// Read file contents into fileContents buffers
		void readFiles();
		
		
		// Query the server using the query address.
		Reply httpQuery() const;
		
		// Perform HTTP post, returning whether successful.
		Reply httpPost() const;
		
		// Test connectivity by making a test connection (but no actual HTTP query)
		bool  httpTest(const ParsedURL &url) const;
		
        
        // Encode HTTP query and post request
		wxString preQueryString()            const;
        void     encodePost(wxHTTP &request) const;
        
    public: // members
		ParsedURL postURL, queryURL;
        
        wxString promptTitle;
        wxString promptMessage;
		
		wxString labelSend, labelCancel, labelView;
		
		wxString dirLabel, dirPath;
        
        bool silent;
		
		bool connectionWarning;
        
        Parameters params;
    };
    
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
            Ev_Submit  = wxID_OK,
            Ev_Details = wxID_VIEW_DETAILS,
        };
        
        Prompt(wxWindow * parent, wxWindowID id, Report &report);
		
		/*
			Display a dialog box describing a server's reply.
				Returns true if the user followed a link.
		*/
		static bool DisplayReply(const Report::Reply &reply, wxWindow *parent);
        
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
        
        Report &report;
        Fields  fields;
        
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
            Ev_Exit = wxID_EXIT,
            Ev_Done = wxID_OK,
            Ev_Open = wxID_OPEN,
        };
        
        ViewReport(wxWindow * parent, wxWindowID id, Report &report);
		virtual ~ViewReport();
		
		static bool Exists();
        
    private:
        void OnDone (wxCommandEvent & event);
        void OnOpen (wxCommandEvent & event);
        void OnClose(wxCloseEvent   & event);
        
        Report &report;
        
        wxDECLARE_EVENT_TABLE();
    };
	
	// Utility functions
	wxString GetTagContents(const wxString &reply, const wxString &tagName);
	
	class TattleApp;
}

#endif /* tattle_h */
