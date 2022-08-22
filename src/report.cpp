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
	config = Json::object();
	config["url"]  = Json::object();
	config["path"] = Json::object();
	config["gui"]  = Json::object();
	config["data"] = Json::object();

	connectionWarning = false;


	{
		static const char version_str[] = {
			'0'+TATTLE_VERSION_MAJOR, '.',
			'0'+TATTLE_VERSION_MINOR, '.',
			'0'+TATTLE_VERSION_PATCH, '\0'
		};

		Report::Parameter param;
		param.type = PARAM_STRING;
		param.preQuery = true;
		param.name = "tattle";
		param.json = {{"value", version_str}};
		params.push_back(std::move(param));
	}
}

void Report::_parse_urls() const
{
	if (config.contains("url"))
	{
		auto& urls = config["url"];
		url_cache.post .set(urls.value("base", "") + urls.value("post",  ""));
		url_cache.query.set(urls.value("base", "") + urls.value("query", ""));
		url_cache.parsed = true;
	}
}

const Report::ParsedURL& Report::url_post() const
{
	//if (!url_cache.parsed)
		_parse_urls();
	return url_cache.post;
}
const Report::ParsedURL& Report::url_query() const
{
	//if (!url_cache.parsed)
		_parse_urls();
	return url_cache.query;
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
	type(PARAM_NONE), preQuery(false)
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
			if (!wxFile::Access(i->path(), wxFile::read))
			{
				DumpString(i->fileContents, wxT("[File not found]"));
				continue;
			}

			// Open the file
			wxFile file(i->path());
			
			if (!file.IsOpened())
			{
				DumpString(i->fileContents, wxT("[File not found]"));
				continue;
			}
			
			// Clear any pre-existing contents
			i->fileContents.Clear();
			
			// Read the file into the post buffer directly
			wxFileOffset fileLength = file.Length();

			auto
				trunc_begin = i->truncate_begin(),
				trunc_end   = i->truncate_end();
			auto
				trunc_note = i->truncate_note();
			
			if ((trunc_begin || trunc_end) && (trunc_begin+trunc_end < fileLength))
			{
				// Trim the file
				if (trunc_begin)
				{
					size_t consumed = file.Read(i->fileContents.GetAppendBuf(trunc_begin), trunc_begin);
					i->fileContents.UngetAppendBuf(consumed);
					
					DumpString(i->fileContents, " ...\r\n\r\n");
				}
				if (trunc_note.length())
				{
					DumpString(i->fileContents, trunc_note);
				}
				if (trunc_end)
				{
					DumpString(i->fileContents, "\r\n\r\n... ");
					file.Seek(-wxFileOffset(trunc_end), wxFromEnd);
					size_t consumed = file.Read(i->fileContents.GetAppendBuf(trunc_end), trunc_end);
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
	wxString boundary_divider = "\r\n--" + boundary_id + "\r\n";
	
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
		if (i->path().length())
		{
			// Filename
			DumpString(postBuffer, "; filename=\"");
			DumpString(postBuffer, i->path());
			DumpString(postBuffer, "\"");
		}
		DumpString(postBuffer, "\r\n");
		
		// Additional content type info
		if (i->content_info().length())
		{
			DumpString(postBuffer, i->content_info());
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
			DumpString(postBuffer, i->value());
			break;
		
		case PARAM_FILE_TEXT:
		case PARAM_FILE_BIN:
			DumpBuffer(postBuffer, i->fileContents);
			break;
		case PARAM_NONE:
		default:
			DumpString(postBuffer, "Unknown Data:\r\n");
			DumpString(postBuffer, i->value());
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
			size_t len = std::min<size_t>(i->value().length(), 64);
			for (size_t n = 0; n < len; ++n)
			{
				wxUniChar uc = i->value()[n];
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
