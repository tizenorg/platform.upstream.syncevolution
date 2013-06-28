/*
 * Copyright (C) 2009 Patrick Ohly <patrick.ohly@gmx.de>
 * Copyright (C) 2009 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef INCL_LOGSTDOUT
#define INCL_LOGSTDOUT

#include <syncevo/Logging.h>
#include <syncevo/util.h>
#include <stdio.h>
#include <string>

#include <syncevo/declarations.h>
SE_BEGIN_CXX


/**
 * A logger which writes to stdout or a file.
 */
class LoggerStdout : public Logger
{
    FILE *m_file;
    bool m_closeFile;

 public:
    /**
     * write to stdout by default
     *
     * @param file    override default file; NULL disables printing
     */
    LoggerStdout(FILE *file = stdout);

    /**
     * open and own the given log file
     *
     * @param filename     will be opened relative to current directory
     */
    LoggerStdout(const std::string &filename);

    ~LoggerStdout();

    void write(FILE *file,
               Level msglevel,
               Level filelevel,
               const std::string *prefix,
               const std::string *procname,
               const char *format,
               va_list args);
    virtual void messagev(const MessageOptions &options,
                          const char *format,
                          va_list args);
};

SE_END_CXX
#endif // INCL_LOGSTDOUT
