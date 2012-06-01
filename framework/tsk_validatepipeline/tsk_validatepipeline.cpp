/*
*
*  The Sleuth Kit
*
*  Contact: Brian Carrier [carrier <at> sleuthkit [dot] org]
*  Copyright (c) 2010-2012 Basis Technology Corporation. All Rights
*  reserved.
*
*  This software is distributed under the Common Public License 1.0
*/
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <Windows.h>

#include "framework.h"
#include "Services/TskSystemPropertiesImpl.h"

#include "Poco/AutoPtr.h"
#include "Poco/Path.h"
#include "Poco/UnicodeConverter.h"
#include "Poco/DOM/DOMParser.h"
#include "Poco/DOM/Document.h"
#include "Poco/DOM/NodeList.h"
#include "Poco/DOM/NodeIterator.h"
#include "Poco/DOM/DOMWriter.h"
#include "Poco/SAX/InputSource.h"
#include "Poco/SAX/SAXException.h"
#include "Poco/Util/XMLConfiguration.h"
#include "Poco/Util/AbstractConfiguration.h"


#define VALIDATE_PIPELINE_VERSION "1.0.0.0"

class ValidatePipeline
{
public:
    ValidatePipeline();
    ~ValidatePipeline();

    bool isValid(const char * a_configPath) const;

private:
    const char *m_configPath;
};

static char *progname;

static void usage()
{
    fprintf(stderr, "Usage: %s framework_config_file pipeline_config_file \n", progname);
    fprintf(stderr, "\tframework_config_file: Framework config file that identifies where module directory, etc. is found.\n");
    fprintf(stderr, "\tpipeline_config_file: Pipeline config file to validate.\n");
}

ValidatePipeline::ValidatePipeline()
{
}

ValidatePipeline::~ValidatePipeline()
{
}

/* Validate all of the pipelines in the given config file. 
* This method does some basic parsing of the config file to learn
* about the various pipelines that exist in the file.
*/
bool ValidatePipeline::isValid(const char *a_configPath) const
{
    bool result = false;
    std::ifstream in(a_configPath);
    if (!in) {
        fprintf(stdout, "Error opening pipeline config file: %s\n", a_configPath);
    } else {
        try {
            Poco::XML::InputSource src(in);
            Poco::XML::DOMParser parser;
            // basic parsing
            Poco::AutoPtr<Poco::XML::Document> xmlDoc = parser.parse(&src);

            // must have at least one pipeline element
            Poco::AutoPtr<Poco::XML::NodeList> pipelines = 
                xmlDoc->getElementsByTagName(TskPipelineManager::PIPELINE_ELEMENT);

            if (pipelines->length() == 0) {
                fprintf(stdout, "No pipelines found in config file.\n");
            } else {
                bool failed = false;
                // parse all pipelines in the config file
                for (unsigned long i = 0; i < pipelines->length(); i++)
                {
                    Poco::XML::Node * pNode = pipelines->item(i);
                    Poco::XML::Element* pElem = dynamic_cast<Poco::XML::Element*>(pNode);
                    Poco::XML::DOMWriter writer;
                    std::ostringstream pipelineXml;
                    writer.writeNode(pipelineXml, pNode);

                    std::string pipelineType = pElem->getAttribute(TskPipelineManager::PIPELINE_TYPE);

                    TskPipeline * pipeline;
                    if (pipelineType == TskPipelineManager::FILE_ANALYSIS_PIPELINE)
                        pipeline = new TskFileAnalysisPipeline();
                    else if (pipelineType == TskPipelineManager::REPORTING_PIPELINE)
                        pipeline = new TskReportPipeline();
                    else
                        fprintf(stdout, "Unsupported pipeline type: %s\n", pipelineType.c_str());

                    try {
                        pipeline->validate(pipelineXml.str());
                    } catch (...) {
                        fprintf(stdout, "Error parsing pipeline: %s\n", pElem->getAttribute(TskPipelineManager::PIPELINE_TYPE).c_str());
                        failed = true;
                    }
                    delete pipeline;
                }
                if (!failed)
                    result = true;
            }
        } catch (Poco::XML::SAXParseException& ex) {
            fprintf(stderr, "Error parsing pipeline config file: %s (%s)\n", a_configPath, ex.what());
        }

    }
    return result;
}

static std::wstring getProgDir()
{
    wchar_t progPath[256];
    wchar_t fullPath[256];
    HINSTANCE hInstance = GetModuleHandle(NULL);

    GetModuleFileName(hInstance, fullPath, 256);
    for (int i = wcslen(fullPath)-1; i > 0; i--) {
        if (i > 256)
            break;

        if (fullPath[i] == '\\') {
            wcsncpy_s(progPath, fullPath, i+1);
            progPath[i+1] = '\0';
            break;
        }
    }
    return std::wstring(progPath);
}

int main(int argc, char **argv)
{
    progname = argv[0];
    if (argc != 3) {
        usage();
        return 1;
    }
    char *frameworkConfigPath = argv[1];
    char *pipelineConfigPath = argv[2];

    fprintf(stderr, "Validating %s\n", pipelineConfigPath);

    // open the log temp file
    Log log;
    wchar_t filename[MAX_PATH];

    if (!GetTempFileName(L".", L"", 0, filename)) {
        fprintf(stderr, "Failed to create temporary file.\n");
        return 1;
    }
    log.open(filename);
    TskServices::Instance().setLog(log);

    std::wstring progDirPath = getProgDir();

    // Initialize properties based on the config file. Do this to shutdown noise in validation.
    TskSystemPropertiesImpl systemProperties;
    systemProperties.initialize(frameworkConfigPath);
    TskServices::Instance().setSystemProperties(systemProperties);

    TSK_SYS_PROP_SET(TskSystemProperties::PROG_DIR, progDirPath); 

    ValidatePipeline vp;
    bool valid = vp.isValid(pipelineConfigPath);
    fprintf(stdout, "%s is %s\n", pipelineConfigPath, (valid ? "valid." : "invalid."));

    // close the log file and dump content to stdout
    log.close();

#define MAX_BUF 1024
    char buf[MAX_BUF];
    std::ifstream fin(filename);

    fprintf(stdout, "\nLog messages created during validation: \n");
    while (fin.getline(buf, MAX_BUF)) {
        fprintf(stdout, "%s\n", buf);
    }
    fin.close();

    (void)DeleteFile(filename);

    if (valid)
        return 0;
    else
        return 1;
}
