/*
 *
 *  The Sleuth Kit
 *
 *  Contact: Brian Carrier [carrier <at> sleuthkit [dot] org]
 *  Copyright (c) 2010-2011 Basis Technology Corporation. All Rights
 *  reserved.
 *
 *  This software is distributed under the Common Public License 1.0
 */

/**
 * \file TskExecutableModule.cpp
 * Contains the implementation for the TskExecutableModule class.
 */

// System includes
#include <sstream>

// Framework includes
#include "TskExecutableModule.h"
#include "Services/TskServices.h"
#include "Utilities/TskException.h"
#include "File/TskFileManagerImpl.h"

// Poco includes
#include "Poco/String.h"
#include "Poco/StringTokenizer.h"
#include "Poco/FileStream.h"
#include "Poco/Process.h"
#include "Poco/PipeStream.h"
#include "Poco/StreamCopier.h"
#include "Poco/Path.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/DateTimeFormat.h"

/**
 * Constructor
 */
TskExecutableModule::TskExecutableModule() : m_output("")
{
}

/**
 * Destructor
 */
TskExecutableModule::~TskExecutableModule()
{
}

/**
 * Run the module on the given file.
 */
TskModule::Status TskExecutableModule::run(TskFile* fileToAnalyze)
{
    try
    {
        if (fileToAnalyze == NULL)
        {
            LOGERROR(L"TskExecutableModule::run - Passed NULL file pointer.");
            throw TskNullPointerException();
        }

        // Perform parameter substitution on command line args.
        std::string arguments = parameterSubstitution(m_arguments, fileToAnalyze);

        // Split the arguments into a vector of strings.
        Poco::StringTokenizer tokenizer(arguments, " ");

        std::vector<std::string> vectorArgs(tokenizer.begin(), tokenizer.end());

        // Perform parameter substitution on our output location
        std::string outFilePath = parameterSubstitution(m_output, fileToAnalyze);

        // If an output file has been specified we need to ensure that anything
        // written to stdout gets put in the file. This is accomplished by passing
        // a pipe to Poco::Process::launch and reading its contents once the process
        // has terminated.
        if (!outFilePath.empty())
        {
            // Create directories that may be missing along the path.
            Poco::Path outPath(outFilePath);
            Poco::File outDir(outPath.parent());
            outDir.createDirectories();

            // Create the output file if it does not exist.
            Poco::File outFile(outFilePath);

            if (!outFile.exists())
            {
                outFile.createFile();
            }

            // Create process redirecting its output to a Pipe.
            Poco::Pipe outPipe;

            Poco::ProcessHandle handle = Poco::Process::launch(m_modulePath, vectorArgs, NULL, &outPipe, NULL);
            
            // Copy output from Pipe to the output file.
            Poco::PipeInputStream istr(outPipe);
            Poco::FileOutputStream ostr(outFile.path(), std::ios::out|std::ios::app);

            while (istr)
            {
                Poco::StreamCopier::copyStream(istr, ostr);
            }

            // The process should be finished. Check its exit code.
            int exitCode = Poco::Process::wait(handle);

            if (exitCode != 0)
            {
                // If a module fails we log a warning message and continue.
                std::wstringstream msg;
                msg << L"TskExecutableModule::run - Module (" << m_modulePath.c_str()
                    << L") failed with exit code: " << exitCode << std::endl;
                LOGWARN(msg.str());
            }
        }
        else
        {
            // No output file was specified.
            Poco::ProcessHandle handle = Poco::Process::launch(m_modulePath, vectorArgs);

            // Wait for the process to complete
            int exitCode = Poco::Process::wait(handle);

            if (exitCode != 0)
            {
                // If a module fails we log a warning message and continue.
                std::wstringstream msg;
                msg << L"TskExecutableModule::run - Module (" << m_modulePath.c_str()
                    << L") failed with exit code: " << exitCode << std::endl;
                LOGWARN(msg.str());
            }
        }
    }
    catch (Poco::Exception& ex)
    {
        std::wstringstream errorMsg;
        errorMsg << L"TskExecutableModule::run - Error: " << ex.displayText().c_str() << std::endl;
        LOGERROR(errorMsg.str());
        throw TskException("Module execution failed.");
    }

    return TskModule::OK;
}

/**
 * Confirm that an executable file exists at location.
 */
void TskExecutableModule::setPath(const std::string& location)
{
    try
    {
        // Call our parent to validate the location.
        TskModule::setPath(location);

        // Verify that the file is executable.
        Poco::File exeFile(m_modulePath);

        if (!exeFile.canExecute())
        {
            std::wstringstream msg;
            msg << L"TskExecutableModule::setPath - File is not executable: "
                << m_modulePath.c_str() << std::endl;
            LOGERROR(msg.str());
            throw TskException("File is not executable.");
        }
    }
    catch(std::exception& ex)
    {
        // Log a message and throw a framework exception.
        std::wstringstream msg;
        msg << "TskExecutableModule::setPath : " << ex.what() << std::endl;
        LOGERROR(msg.str());

        throw new TskException("Failed to set location: " + m_modulePath);
    }
}

/**
 *
 */
void TskExecutableModule::setOutput(const std::string& outFile)
{
    m_output = outFile;
}

/**
 *
 */
std::string TskExecutableModule::getOutput() const
{
    return m_output;
}

