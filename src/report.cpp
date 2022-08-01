//
//  report.cpp
//  tattle
//
//  Created by Evan Balster on 11/12/16.
//

#include <wx/defs.h>

#include <cstring>
#include <algorithm>

#include "tattle.h"

#include <wx/file.h>


using namespace tattle;


Report::Report()
{
	connectionWarning = false;
}

Report::Parameter *Report::findParam(const wxString &name)
{
	for (Parameters::iterator i = params.begin(); i != params.end(); ++i) if (i->name == name) return &*i;
	return NULL;
}

const Report::Parameter *Report::findParam(const wxString &name) const
{
	for (Parameters::const_iterator i = params.begin(); i != params.end(); ++i) if (i->name == name) return &*i;
	return NULL;
}


Report::Parameter::Parameter() :
	type(PARAM_NONE), preQuery(false), trimBegin(0), trimEnd(0)
{
}


static void DumpString(wxMemoryBuffer &dest, const char *src)
{
	dest.AppendData((void*) src, std::strlen(src));
}

static void DumpString(wxMemoryBuffer &dest, const std::string src)
{
	dest.AppendData((void*) &src[0], src.length());
}

static void DumpString(wxMemoryBuffer &dest, const wxString &src)
{
	wxScopedCharBuffer utf8 = src.ToUTF8();
	dest.AppendData(utf8.data(), utf8.length());
}

static void DumpBuffer(wxMemoryBuffer &dest, const wxMemoryBuffer &src)
{
	dest.AppendData(src.GetData(), src.GetDataLen());
}

void Report::readFiles()
{
	for (Parameters::iterator i = params.begin(); i != params.end(); ++i)
	{
		if (i->type == PARAM_FILE_TEXT || i->type == PARAM_FILE_BIN)
		{
			if (!wxFile::Access(i->fname, wxFile::read))
			{
				DumpString(i->fileContents, wxT("[File not found]"));
				continue;
			}

			// Open the file
			wxFile file(i->fname);
			
			if (!file.IsOpened())
			{
				DumpString(i->fileContents, wxT("[File not found]"));
				continue;
			}
			
			// Clear any pre-existing contents
			i->fileContents.Clear();
			
			// Read the file into the post buffer directly
			wxFileOffset fileLength = file.Length();
			
			if ((i->trimBegin || i->trimEnd) && (i->trimBegin+i->trimEnd < fileLength))
			{
				// Trim the file
				if (i->trimBegin)
				{
					size_t consumed = file.Read(i->fileContents.GetAppendBuf(i->trimBegin), i->trimBegin);
					i->fileContents.UngetAppendBuf(consumed);
					
					DumpString(i->fileContents, " ...\r\n\r\n");
				}
				if (i->trimNote.Length())
				{
					DumpString(i->fileContents, i->trimNote);
				}
				if (i->trimEnd)
				{
					DumpString(i->fileContents, "\r\n\r\n... ");
					file.Seek(-wxFileOffset(i->trimEnd), wxFromEnd);
					size_t consumed = file.Read(i->fileContents.GetAppendBuf(i->trimEnd), i->trimEnd);
					i->fileContents.UngetAppendBuf(consumed);
				}
			}
			else
			{
				size_t consumed = file.Read(i->fileContents.GetAppendBuf(fileLength), fileLength);
				i->fileContents.UngetAppendBuf(consumed);
			}
		}
	}
}



void Report::encodePost(wxMemoryBuffer &postBuffer, wxString boundary_id, bool preQuery) const
{
	// Boundary beginning with two hyphen-minus characters
	std::string boundary_divider = "\r\n--" + boundary_id + "\r\n";
	
	for (Parameters::const_iterator i = params.begin(); true; ++i)
	{
		if (preQuery && i != params.end())
		{
			// Only send strings marked for pre-querying
			if (!i->preQuery || i->type != PARAM_STRING) continue;
		}
	
		// All items are preceded and succeeded by a boundary.
		DumpString(postBuffer, boundary_divider);
	
		// Not the last item, is it?
		if (i == params.end()) break;
	
		// Content disposition and name
		DumpString(postBuffer, "Content-Disposition: form-data; name=\"");
		DumpString(postBuffer, i->name);
		DumpString(postBuffer, "\"");
		if (i->fname.length())
		{
			// Filename
			DumpString(postBuffer, "; filename=\"");
			DumpString(postBuffer, i->fname);
			DumpString(postBuffer, "\"");
		}
		DumpString(postBuffer, "\r\n");
		
		// Additional content type info
		if (i->contentInfo.length())
		{
			DumpString(postBuffer, i->contentInfo);
			DumpString(postBuffer, "\r\n");
		}
		
		// Empty line...
		DumpString(postBuffer, "\r\n");
		
		// Content...
		switch (i->type)
		{
		case PARAM_STRING:
		case PARAM_FIELD:
		case PARAM_FIELD_MULTI:
			DumpString(postBuffer, i->value);
			break;
		
		case PARAM_FILE_TEXT:
		case PARAM_FILE_BIN:
			DumpBuffer(postBuffer, i->fileContents);
			break;
		case PARAM_NONE:
		default:
			DumpString(postBuffer, "Unknown Data:\r\n");
			DumpString(postBuffer, i->value);
			break;
		}
	}
	
	postBuffer.AppendByte('\0');
	cout << "HTTP Post Buffer:" << endl << ((const char*) postBuffer.GetData()) << endl;
}

static void AppendPercentEncoded(wxString &str, char c)
{
	str.append('%');
	str.append("0123456789ABCDEF"[(int(c)>>4)&15]);
	str.append("0123456789ABCDEF"[(int(c)   )&15]);
}

wxString Report::preQueryString() const
{
	wxString query;
	
	for (Parameters::const_iterator i = params.begin(); i != params.end(); ++i)
	{
		if (i->preQuery)
		{
			query += (query.Length() ? wxT("&") : wxT("?"));
			query += i->name;
			query += "=";
			
			// Quick and dirty URL encoding
			size_t len = std::min<size_t>(i->value.Length(), 64);
			for (size_t n = 0; n < len; ++n)
			{
				wxUniChar uc = i->value.GetChar(n);
				char c;
				if (uc.IsAscii() && uc.GetAsChar(&c))
				{
					if (c >= '0' && c <= '9') {query.Append(uc); continue;}
					if (c >= 'a' && c <= 'z') {query.Append(uc); continue;}
					if (c >= 'A' && c <= 'Z') {query.Append(uc); continue;}
					if (c=='.'||c=='-'||c=='_'||c=='~') {query.append(uc); continue;}
					
					AppendPercentEncoded(query, c);
				}
				else
				{
					// Encode in UTF-8 and percent encode that
					wxString tmp; tmp.append(uc);
					wxScopedCharBuffer utf8 = tmp.ToUTF8();
					
					for (size_t m = 0; m < utf8.length(); ++m)
					{
						AppendPercentEncoded(query, utf8[m]);
					}
				}
			}
			
		}
	}
	
	return query;
}
