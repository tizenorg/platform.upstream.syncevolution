/*
 * Copyright (C) 2008-2009 Patrick Ohly <patrick.ohly@gmx.de>
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

#include <syncevo/Cmdline.h>
#include <syncevo/FilterConfigNode.h>
#include <syncevo/VolatileConfigNode.h>
#include <syncevo/SyncSource.h>
#include <syncevo/SyncContext.h>
#include <syncevo/util.h>
#include "test.h"

#include <unistd.h>
#include <errno.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <memory>
#include <set>
#include <algorithm>
using namespace std;

#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

#include <syncevo/declarations.h>
SE_BEGIN_CXX

Cmdline::Cmdline(int argc, const char * const * argv, ostream &out, ostream &err) :
    m_argc(argc),
    m_argv(argv),
    m_out(out),
    m_err(err),
    m_validSyncProps(SyncConfig::getRegistry()),
    m_validSourceProps(SyncSourceConfig::getRegistry())
{}

bool Cmdline::parse()
{
    int opt = 1;
    while (opt < m_argc) {
        if (m_argv[opt][0] != '-') {
            break;
        }
        if (boost::iequals(m_argv[opt], "--sync") ||
            boost::iequals(m_argv[opt], "-s")) {
            opt++;
            string param;
            string cmdopt(m_argv[opt - 1]);
            if (!parseProp(m_validSourceProps, m_sourceProps,
                           m_argv[opt - 1], opt == m_argc ? NULL : m_argv[opt],
                           SyncSourceConfig::m_sourcePropSync.getName().c_str())) {
                return false;
            }

            // disable requirement to add --run explicitly in order to
            // be compatible with traditional command lines
            m_run = true;
        } else if(boost::iequals(m_argv[opt], "--sync-property") ||
                  boost::iequals(m_argv[opt], "-y")) {
                opt++;
                if (!parseProp(m_validSyncProps, m_syncProps,
                               m_argv[opt - 1], opt == m_argc ? NULL : m_argv[opt])) {
                    return false;
                }
        } else if(boost::iequals(m_argv[opt], "--source-property") ||
                  boost::iequals(m_argv[opt], "-z")) {
            opt++;
            if (!parseProp(m_validSourceProps, m_sourceProps,
                           m_argv[opt - 1], opt == m_argc ? NULL : m_argv[opt])) {
                return false;
            }
        }else if(boost::iequals(m_argv[opt], "--template") ||
                  boost::iequals(m_argv[opt], "-l")) {
            opt++;
            if (opt >= m_argc) {
                usage(true, string("missing parameter for ") + cmdOpt(m_argv[opt - 1]));
                return false;
            }
            m_template = m_argv[opt];
            m_configure = true;
            string temp = boost::trim_copy (m_template);
            if (temp.find ("?") == 0){
                m_printTemplates = true;
                m_dontrun = true;
                m_template = temp.substr (1);
            }
        } else if(boost::iequals(m_argv[opt], "--print-servers") ||
                  boost::iequals(m_argv[opt], "--print-peers") ||
                  boost::iequals(m_argv[opt], "--print-configs")) {
            m_printServers = true;
        } else if(boost::iequals(m_argv[opt], "--print-config") ||
                  boost::iequals(m_argv[opt], "-p")) {
            m_printConfig = true;
        } else if(boost::iequals(m_argv[opt], "--print-sessions")) {
            m_printSessions = true;
        } else if(boost::iequals(m_argv[opt], "--configure") ||
                  boost::iequals(m_argv[opt], "-c")) {
            m_configure = true;
        } else if(boost::iequals(m_argv[opt], "--remove")) {
            m_remove = true;
        } else if(boost::iequals(m_argv[opt], "--run") ||
                  boost::iequals(m_argv[opt], "-r")) {
            m_run = true;
        } else if(boost::iequals(m_argv[opt], "--restore")) {
            opt++;
            if (opt >= m_argc) {
                usage(true, string("missing parameter for ") + cmdOpt(m_argv[opt - 1]));
                return false;
            }
            m_restore = m_argv[opt];
            if (m_restore.empty()) {
                usage(true, string("missing parameter for ") + cmdOpt(m_argv[opt - 1]));
                return false;
            }
            if (!isDir(m_restore)) {
                usage(true, string("parameter '") + m_restore + "' for " + cmdOpt(m_argv[opt - 1]) + " must be log directory");
                return false;
            }
        } else if(boost::iequals(m_argv[opt], "--before")) {
            m_before = true;
        } else if(boost::iequals(m_argv[opt], "--after")) {
            m_after = true;
        } else if(boost::iequals(m_argv[opt], "--dry-run")) {
            m_dryrun = true;
        } else if(boost::iequals(m_argv[opt], "--migrate")) {
            m_migrate = true;
        } else if(boost::iequals(m_argv[opt], "--status") ||
                  boost::iequals(m_argv[opt], "-t")) {
            m_status = true;
        } else if(boost::iequals(m_argv[opt], "--quiet") ||
                  boost::iequals(m_argv[opt], "-q")) {
            m_quiet = true;
        } else if(boost::iequals(m_argv[opt], "--help") ||
                  boost::iequals(m_argv[opt], "-h")) {
            m_usage = true;
        } else if(boost::iequals(m_argv[opt], "--version")) {
            m_version = true;
        } else if(boost::iequals(m_argv[opt], "--keyring")||
                boost::iequals(m_argv[opt], "-k")) {
            m_keyring = true;
        } else {
            usage(false, string(m_argv[opt]) + ": unknown parameter");
            return false;
        }
        opt++;
    }

    if (opt < m_argc) {
        m_server = m_argv[opt++];
        while (opt < m_argc) {
            m_sources.insert(m_argv[opt++]);
        }
    }

    return true;
}

bool Cmdline::run() {
    // --dry-run is only supported by some operations.
    // Be very strict about it and make sure it is off in all
    // potentially harmful operations, otherwise users might
    // expect it to have an effect when it doesn't.

    if (m_usage) {
        usage(true);
    } else if (m_version) {
        printf("SyncEvolution %s\n", VERSION);
        printf("%s", EDSAbiWrapperInfo());
        printf("%s", SyncSource::backendsInfo().c_str());
    } else if (m_printServers || boost::trim_copy(m_server) == "?") {
        dumpConfigs("Configured servers:",
                    SyncConfig::getConfigs());
    } else if (m_printTemplates) {
        SyncConfig::DeviceList devices;
        if (m_template.empty()){
            dumpConfigTemplates("Available configuration templates:",
                    SyncConfig::getPeerTemplates(devices), false);
        } else {
            //limiting at templates for syncml clients only.
            devices.push_back (SyncConfig::DeviceDescription("", m_template, SyncConfig::MATCH_FOR_SERVER_MODE));
            dumpConfigTemplates("Available configuration templates:",
                    SyncConfig::matchPeerTemplates(devices), true);
        }
    } else if (m_dontrun) {
        // user asked for information
    } else if (m_argc == 1) {
        // no parameters: list databases and short usage
        const SourceRegistry &registry(SyncSource::getSourceRegistry());
        boost::shared_ptr<FilterConfigNode> sharedNode(new VolatileConfigNode());
        boost::shared_ptr<FilterConfigNode> configNode(new VolatileConfigNode());
        boost::shared_ptr<FilterConfigNode> hiddenNode(new VolatileConfigNode());
        boost::shared_ptr<FilterConfigNode> trackingNode(new VolatileConfigNode());
        boost::shared_ptr<FilterConfigNode> serverNode(new VolatileConfigNode());
        SyncSourceNodes nodes(sharedNode, configNode, hiddenNode, trackingNode, serverNode);
        SyncSourceParams params("list", nodes);
        
        BOOST_FOREACH(const RegisterSyncSource *source, registry) {
            BOOST_FOREACH(const Values::value_type &alias, source->m_typeValues) {
                if (!alias.empty() && source->m_enabled) {
                    configNode->setProperty("type", *alias.begin());
                    auto_ptr<SyncSource> source(SyncSource::createSource(params, false));
                    if (source.get() != NULL) {
                        listSources(*source, boost::join(alias, " = "));
                        m_out << "\n";
                    }
                }
            }
        }

        usage(false);
    } else if (m_printConfig) {
        boost::shared_ptr<SyncConfig> config;
        ConfigProps syncFilter;
        SourceFilters_t sourceFilters;

        if (m_template.empty()) {
            if (m_server.empty()) {
                m_err << "ERROR: --print-config requires either a --template or a server name." << endl;
                return false;
            }
            config.reset(new SyncConfig(m_server));
            if (!config->exists()) {
                m_err << "ERROR: server '" << m_server << "' has not been configured yet." << endl;
                return false;
            }

            syncFilter = m_syncProps;
            sourceFilters[""] = m_sourceProps;
        } else {
            string peer, context;
            SyncConfig::splitConfigString(SyncConfig::normalizeConfigString(m_template), peer, context);

            config = SyncConfig::createPeerTemplate(peer);
            if (!config.get()) {
                m_err << "ERROR: no configuration template for '" << m_template << "' available." << endl;
                return false;
            }

            getFilters(context, syncFilter, sourceFilters);
        }

        if (m_sources.empty() ||
            m_sources.find("main") != m_sources.end()) {
            boost::shared_ptr<FilterConfigNode> syncProps(config->getProperties());
            syncProps->setFilter(syncFilter);
            dumpProperties(*syncProps, config->getRegistry(), false);
        }

        list<string> sources = config->getSyncSources();
        sources.sort();
        BOOST_FOREACH(const string &name, sources) {
            if (m_sources.empty() ||
                m_sources.find(name) != m_sources.end()) {
                m_out << endl << "[" << name << "]" << endl;
                SyncSourceNodes nodes = config->getSyncSourceNodes(name);
                boost::shared_ptr<FilterConfigNode> sourceProps = nodes.getProperties();
                SourceFilters_t::const_iterator it = sourceFilters.find(name);
                if (it != sourceFilters.end()) {
                    sourceProps->setFilter(it->second);
                } else {
                    sourceProps->setFilter(sourceFilters[""]);
                }
                dumpProperties(*sourceProps, SyncSourceConfig::getRegistry(),
                               name != *(--sources.end()));
            }
        }
    } else if (m_server == "" && m_argc > 1) {
        // Options given, but no server - not sure what the user wanted?!
        usage(true, "server name missing");
        return false;
    } else if (m_configure || m_migrate) {
        if (m_dryrun) {
            SyncContext::throwError("--dry-run not supported for configuration changes");
        }
        if (m_keyring) {
#ifndef USE_GNOME_KEYRING
            m_err << "Error: this syncevolution binary was compiled without support for storing "
                     "passwords in a keyring. Either store passwords in your configuration "
                     "files or enter them interactively on each program run." << endl;
            return false;
#endif
        }

        bool fromScratch = false;
        string peer, context;
        SyncConfig::splitConfigString(SyncConfig::normalizeConfigString(m_server), peer, context);

        // Both config changes and migration are implemented as copying from
        // another config (template resp. old one). Migration also moves
        // the old config.
        boost::shared_ptr<SyncConfig> from;
        if (m_migrate) {
            string oldContext = context;
            from.reset(new SyncConfig(m_server));
            if (!from->exists()) {
                // for migration into a different context, search for config without context
                oldContext = "";
                from.reset(new SyncConfig(peer));
                if (!from->exists()) {
                    m_err << "ERROR: server '" << m_server << "' has not been configured yet." << endl;
                    return false;
                }
            }

            int counter = 0;
            string oldRoot = from->getRootPath();
            string suffix;
            while (true) {
                string newname;
                ostringstream newsuffix;
                newsuffix << ".old";
                if (counter) {
                    newsuffix << "." << counter;
                }
                suffix = newsuffix.str();
                newname = oldRoot + suffix;
                if (!rename(oldRoot.c_str(),
                            newname.c_str())) {
                    break;
                } else if (errno != EEXIST && errno != ENOTEMPTY) {
                    m_err << "ERROR: renaming " << oldRoot << " to " <<
                        newname << ": " << strerror(errno) << endl;
                    return false;
                }
                counter++;
            }

            from.reset(new SyncConfig(peer + suffix +
                                      (oldContext.empty() ? "" : "@") +
                                      oldContext));
        } else {
            from.reset(new SyncConfig(m_server));
            if (!from->exists()) {
                // creating from scratch, look for template
                fromScratch = true;
                string configTemplate = m_template.empty() ? m_server : m_template;
                SyncConfig::splitConfigString(SyncConfig::normalizeConfigString(configTemplate), peer, context);
                from = SyncConfig::createPeerTemplate(peer);
                if (!from.get()) {
                    m_err << "ERROR: no configuration template for '" << configTemplate << "' available." << endl;
                    dumpConfigTemplates("Available configuration templates:",
                                SyncConfig::getPeerTemplates(SyncConfig::DeviceList()));
                    return false;
                }

                // Templates no longer contain these strings, because
                // GUIs would have to localize them. For configs created
                // via the command line, the extra hint that these
                // properties need to be set is useful, so set these
                // strings here. They'll get copied into the new
                // config only if no other value was given on the
                // command line.
                if (!from->getUsername()[0]) {
                    from->setUsername("your SyncML server account name");
                }
                if (!from->getPassword()[0]) {
                    from->setPassword("your SyncML server password");
                }
            }
        }

        // Apply config changes on-the-fly. Regardless what we do
        // (changing an existing config, migrating, creating from
        // a template), existing shared properties in the desired
        // context must be preserved unless explicitly overwritten.
        // Therefore read those, update with command line properties,
        // then set as filter.
        ConfigProps syncFilter;
        SourceFilters_t sourceFilters;
        getFilters(context, syncFilter, sourceFilters);
        from->setConfigFilter(true, "", syncFilter);
        BOOST_FOREACH(const SourceFilters_t::value_type &entry, sourceFilters) {
            from->setConfigFilter(false, entry.first, entry.second);
        }

        // write into the requested configuration, creating it if necessary
        boost::shared_ptr<SyncContext> to(createSyncClient());
        to->copy(*from, !fromScratch && !m_sources.empty() ? &m_sources : NULL);

        // Sources are active now according to the server default.
        // Disable all sources not selected by user (if any selected)
        // and those which have no database.
        if (fromScratch) {
            list<string> configuredSources = to->getSyncSources();
            set<string> sources = m_sources;
            
            BOOST_FOREACH(const string &source, configuredSources) {
                boost::shared_ptr<PersistentSyncSourceConfig> sourceConfig(to->getSyncSourceConfig(source));
                string disable = "";
                set<string>::iterator entry = sources.find(source);
                bool selected = entry != sources.end();

                if (!m_sources.empty() &&
                    !selected) {
                    disable = "not selected";
                } else {
                    if (entry != sources.end()) {
                        // The command line parameter matched a valid source.
                        // All entries left afterwards must have been typos.
                        sources.erase(entry);
                    }

                    // check whether the sync source works
                    SyncSourceParams params("list", to->getSyncSourceNodes(source));
                    auto_ptr<SyncSource> syncSource(SyncSource::createSource(params, false));
                    if (syncSource.get() == NULL) {
                        disable = "no backend available";
                    } else {
                        try {
                            SyncSource::Databases databases = syncSource->getDatabases();
                            if (databases.empty()) {
                                disable = "no database to synchronize";
                            }
                        } catch (...) {
                            disable = "backend failed";
                        }
                    }
                }

                if (!disable.empty()) {
                    // abort if the user explicitly asked for the sync source
                    // and it cannot be enabled, otherwise disable it silently
                    if (selected) {
                        SyncContext::throwError(source + ": " + disable);
                    }
                    sourceConfig->setSync("disabled");
                } else if (selected) {
                    // user absolutely wants it: enable even if off by default
                    FilterConfigNode::ConfigFilter::const_iterator sync =
                        m_sourceProps.find(SyncSourceConfig::m_sourcePropSync.getName());
                    sourceConfig->setSync(sync == m_sourceProps.end() ? "two-way" : sync->second);
                }
            }

            if (!sources.empty()) {
                SyncContext::throwError(string("no such source(s): ") + boost::join(sources, " "));
            }
        }
        // give a change to do something before flushing configs to files
        to->preFlush(*to);

        // done, now write it
        to->flush();

        // also copy .synthesis dir?
        if (m_migrate) {
            string fromDir, toDir;
            fromDir = from->getRootPath() + "/.synthesis";
            toDir = to->getRootPath() + "/.synthesis";
            if (isDir(fromDir)) {
                cp_r(fromDir, toDir);
            }
        }
    } else if (m_remove) {
        if (m_dryrun) {
            SyncContext::throwError("--dry-run not supported for removing configurations");
        }

        // extra sanity check
        if (!m_sources.empty() ||
            !m_syncProps.empty() ||
            !m_sourceProps.empty()) {
            usage(true, "too many parameters for --remove");
            return false;
        } else {
            boost::shared_ptr<SyncConfig> config;
            config.reset(new SyncConfig(m_server));
            if (!config->exists()) {
                SyncContext::throwError(string("no such configuration: ") + m_server);
            }
            config->remove();
            return true;
        }
    } else {
        std::set<std::string> unmatchedSources;
        boost::shared_ptr<SyncContext> context;
        context.reset(createSyncClient());
        context->setQuiet(m_quiet);
        context->setDryRun(m_dryrun);
        context->setConfigFilter(true, "", m_syncProps);
        if (m_sources.empty()) {
            if (m_sourceProps.empty()) {
                // empty source list, empty source filter => run with
                // existing configuration without filtering it
            } else {
                // Special semantic of 'no source selected': apply
                // filter only to sources which are
                // *active*. Configuration of inactive sources is left
                // unchanged. This way we don't activate sync sources
                // accidentally when the sync mode is modified
                // temporarily.
                BOOST_FOREACH(const std::string &source,
                              context->getSyncSources()) {
                    boost::shared_ptr<PersistentSyncSourceConfig> source_config =
                        context->getSyncSourceConfig(source);
                    if (strcmp(source_config->getSync(), "disabled")) {
                        context->setConfigFilter(false, source, m_sourceProps);
                    }
                }
            }
        } else {
            // apply (possibly empty) source filter to selected sources
            BOOST_FOREACH(const std::string &source,
                          m_sources) {
                boost::shared_ptr<PersistentSyncSourceConfig> source_config =
                        context->getSyncSourceConfig(source);
                if (!source_config || !source_config->exists()) {
                    // invalid source name in m_sources, remember and
                    // report this below
                    unmatchedSources.insert(source);
                } else if (m_sourceProps.find(SyncSourceConfig::m_sourcePropSync.getName()) ==
                           m_sourceProps.end()) {
                    // Sync mode is not set, must override the
                    // "sync=disabled" set below with the original
                    // sync mode for the source or (if that is also
                    // "disabled") with "two-way". The latter is part
                    // of the command line semantic that listing a
                    // source activates it.
                    FilterConfigNode::ConfigFilter filter = m_sourceProps;
                    string sync = source_config->getSync();
                    filter[SyncSourceConfig::m_sourcePropSync.getName()] =
                        sync == "disabled" ? "two-way" : sync;
                    context->setConfigFilter(false, source, filter);
                } else {
                    // sync mode is set, can use m_sourceProps
                    // directly to apply it
                    context->setConfigFilter(false, source, m_sourceProps);
                }
            }

            // temporarily disable the rest
            FilterConfigNode::ConfigFilter disabled;
            disabled[SyncSourceConfig::m_sourcePropSync.getName()] = "disabled";
            context->setConfigFilter(false, "", disabled);
        }

        // check whether there were any sources specified which do not exist
        if (unmatchedSources.size()) {
            context->throwError(string("no such source(s): ") + boost::join(unmatchedSources, " "));
        }

        if (m_status) {
            context->status();
        } else if (m_printSessions) {
            vector<string> dirs;
            context->getSessions(dirs);
            bool first = true;
            BOOST_FOREACH(const string &dir, dirs) {
                if (first) {
                    first = false;
                } else if(!m_quiet) {
                    cout << endl;
                }
                cout << dir << endl;
                if (!m_quiet) {
                    SyncReport report;
                    context->readSessionInfo(dir, report);
                    cout << report;
                }
            }
        } else if (!m_restore.empty()) {
            // sanity checks: either --after or --before must be given, sources must be selected
            if ((!m_after && !m_before) ||
                (m_after && m_before)) {
                usage(false, "--restore <log dir> must be used with either --after (restore database as it was after that sync) or --before (restore data from before sync)");
                return false;
            }
            if (m_sources.empty()) {
                usage(false, "Sources must be selected explicitly for --restore to prevent accidental restore.");
                return false;
            }
            context->restore(m_restore,
                             m_after ?
                             SyncContext::DATABASE_AFTER_SYNC :
                             SyncContext::DATABASE_BEFORE_SYNC);
        } else {
            if (m_dryrun) {
                SyncContext::throwError("--dry-run not supported for running a synchronization");
            }

            // safety catch: if props are given, then --run
            // is required
            if (!m_run &&
                (m_syncProps.size() || m_sourceProps.size())) {
                usage(false, "Properties specified, but neither '--configure' nor '--run' - what did you want?");
                return false;
            }

            return (context->sync() == STATUS_OK);
        }
    }

    return true;
}

string Cmdline::cmdOpt(const char *opt, const char *param)
{
    string res = "'";
    res += opt;
    if (param) {
        res += " ";
        res += param;
    }
    res += "'";
    return res;
}

bool Cmdline::parseProp(const ConfigPropertyRegistry &validProps,
                                     FilterConfigNode::ConfigFilter &props,
                                     const char *opt,
                                     const char *param,
                                     const char *propname)
{
    if (!param) {
        usage(true, string("missing parameter for ") + cmdOpt(opt, param));
        return false;
    } else if (boost::trim_copy(string(param)) == "?") {
        m_dontrun = true;
        if (propname) {
            return listPropValues(validProps, propname, opt);
        } else {
            return listProperties(validProps, opt);
        }
    } else {
        string propstr;
        string paramstr;
        if (propname) {
            propstr = propname;
            paramstr = param;
        } else {
            const char *equal = strchr(param, '=');
            if (!equal) {
                usage(true, string("the '=<value>' part is missing in: ") + cmdOpt(opt, param));
                return false;
            }
            propstr.assign(param, equal - param);
            paramstr.assign(equal + 1);
        }

        boost::trim(propstr);
        boost::trim_left(paramstr);

        if (boost::trim_copy(paramstr) == "?") {
            m_dontrun = true;
            return listPropValues(validProps, propstr, cmdOpt(opt, param));
        } else {
            const ConfigProperty *prop = validProps.find(propstr);
            if (!prop) {
                m_err << "ERROR: " << cmdOpt(opt, param) << ": no such property" << endl;
                return false;
            } else {
                string error;
                if (!prop->checkValue(paramstr, error)) {
                    m_err << "ERROR: " << cmdOpt(opt, param) << ": " << error << endl;
                    return false;
                } else {
                    props[propstr] = paramstr;
                    return true;                        
                }
            }
        }
    }
}

bool Cmdline::listPropValues(const ConfigPropertyRegistry &validProps,
                                          const string &propName,
                                          const string &opt)
{
    const ConfigProperty *prop = validProps.find(propName);
    if (!prop) {
        m_err << "ERROR: "<< opt << ": no such property" << endl;
        return false;
    } else {
        m_out << opt << endl;
        string comment = prop->getComment();

        if (comment != "") {
            list<string> commentLines;
            ConfigProperty::splitComment(comment, commentLines);
            BOOST_FOREACH(const string &line, commentLines) {
                m_out << "   " << line << endl;
            }
        } else {
            m_out << "   no documentation available" << endl;
        }
        return true;
    }
}

bool Cmdline::listProperties(const ConfigPropertyRegistry &validProps,
                                          const string &opt)
{
    // The first of several related properties has a comment.
    // Remember that comment and print it as late as possible,
    // that way related properties preceed their comment.
    string comment;
    BOOST_FOREACH(const ConfigProperty *prop, validProps) {
        if (!prop->isHidden()) {
            string newComment = prop->getComment();

            if (newComment != "") {
                if (!comment.empty()) {
                    dumpComment(m_out, "   ", comment);
                    m_out << endl;
                }
                comment = newComment;
            }
            m_out << prop->getName() << ":" << endl;
        }
    }
    dumpComment(m_out, "   ", comment);
    return true;
}

void Cmdline::getFilters(const string &context,
                         ConfigProps &syncFilter,
                         map<string, ConfigProps> &sourceFilters)
{
    // Read from context. If it does not exist, we simply set no properties
    // as filter. Previously there was a check for existance, but that was
    // flawed because it ignored the global property "defaultPeer".
    boost::shared_ptr<SyncConfig> shared(new SyncConfig(string("@") + context));
    shared->getProperties()->readProperties(syncFilter);
    BOOST_FOREACH(StringPair entry, m_syncProps) {
        syncFilter[entry.first] = entry.second;
    }

    BOOST_FOREACH(std::string source, shared->getSyncSources()) {
        SyncSourceNodes nodes = shared->getSyncSourceNodes(source, "");
        ConfigProps &props = sourceFilters[source];
        nodes.getProperties()->readProperties(props);

        // Special case "type" property: the value in the context
        // is not preserved. Every new peer must ensure that
        // its own value is compatible (= same backend) with
        // the other peers.
        props.erase("type");

        BOOST_FOREACH(StringPair entry, m_sourceProps) {
            props[entry.first] = entry.second;
        }
    }
    sourceFilters[""] = m_sourceProps;
}

void Cmdline::listSources(SyncSource &syncSource, const string &header)
{
    m_out << header << ":\n";
    SyncSource::Databases databases = syncSource.getDatabases();

    BOOST_FOREACH(const SyncSource::Database &database, databases) {
        m_out << "   " << database.m_name << " (" << database.m_uri << ")";
        if (database.m_isDefault) {
            m_out << " <default>";
        }
        m_out << endl;
    }
}

void Cmdline::dumpConfigs(const string &preamble,
                                       const SyncConfig::ConfigList &servers)
{
    m_out << preamble << endl;
    BOOST_FOREACH(const SyncConfig::ConfigList::value_type &server,servers) {
        m_out << "   "  << server.first << " = " << server.second <<endl; 
    }
    if (!servers.size()) {
        m_out << "   none" << endl;
    }
}

void Cmdline::dumpConfigTemplates(const string &preamble,
                                       const SyncConfig::TemplateList &templates,
                                       bool printRank)
{
    m_out << preamble << endl;
    BOOST_FOREACH(const SyncConfig::TemplateList::value_type server,templates) {
        m_out << "   "  << server->m_name << " = " << server->m_description;
        if (printRank){
            m_out << "    " << server->m_rank;
        }
        m_out << endl;
    }
    if (!templates.size()) {
        m_out << "   none" << endl;
    }
}

void Cmdline::dumpProperties(const ConfigNode &configuredProps,
                             const ConfigPropertyRegistry &allProps,
                             bool hideLegend)
{
    list<string> perPeer, perContext, global;

    BOOST_FOREACH(const ConfigProperty *prop, allProps) {
        if (prop->isHidden()) {
            continue;
        }
        if (!m_quiet) {
            string comment = prop->getComment();
            if (!comment.empty()) {
                m_out << endl;
                dumpComment(m_out, "# ", comment);
            }
        }
        bool isDefault;
        prop->getProperty(configuredProps, &isDefault);
        if (isDefault) {
            m_out << "# ";
        }
        m_out << prop->getName() << " = " << prop->getProperty(configuredProps) << endl;

        list<string> *type = NULL;
        switch (prop->getSharing()) {
        case ConfigProperty::GLOBAL_SHARING:
            type = &global;
            break;
        case ConfigProperty::SOURCE_SET_SHARING:
            type = &perContext;
            break;
        case ConfigProperty::NO_SHARING:
            type = &perPeer;
            break;
        }
        if (type) {
            type->push_back(prop->getName());
        }
    }

    if (!m_quiet && !hideLegend) {
        if (!perPeer.empty() ||
            !perContext.empty() ||
            !global.empty()) {
            m_out << endl;
        }
        if (!perPeer.empty()) {
            m_out << "# per-peer (unshared) properties: " << boost::join(perPeer, ", ") << endl;
        }
        if (!perContext.empty()) {
            m_out << "# shared by peers in same context: " << boost::join(perContext, ", ") << endl;
        }
        if (!global.empty()) {
            m_out << "# global properties: " << boost::join(global, ", ") << endl;
        }
    }
}

void Cmdline::dumpComment(ostream &stream,
                                       const string &prefix,
                                       const string &comment)
{
    list<string> commentLines;
    ConfigProperty::splitComment(comment, commentLines);
    BOOST_FOREACH(const string &line, commentLines) {
        stream << prefix << line << endl;
    }
}

void Cmdline::usage(bool full, const string &error, const string &param)
{
    ostream &out(error.empty() ? m_out : m_err);

    out << "Show available sources:" << endl;
    out << "  " << m_argv[0] << endl;
    out << "Show information about configuration(s) and sync sessions:" << endl;
    out << "  " << m_argv[0] << " --print-servers|--print-configs|--print-peers" << endl;
    out << "  " << m_argv[0] << " --print-config [--quiet] <config> [main|<source ...]" << endl;
    out << "  " << m_argv[0] << " --print-sessions [--quiet] <config>" << endl;
    out << "Show information about SyncEvolution:" << endl;
    out << "  " << m_argv[0] << " --help|-h" << endl;
    out << "  " << m_argv[0] << " --version" << endl;
    out << "Run a synchronization:" << endl;
    out << "  " << m_argv[0] << " <config> [<source> ...]" << endl;
    out << "  " << m_argv[0] << " --run <options for run> <config> [<source> ...]" << endl;
    out << "Restore data from the automatic backups:" << endl;
    out << "  " << m_argv[0] << " --restore <session directory> --before|--after [--dry-run] <config> <source> ..." << endl;
    out << "Remove a configuration:" << endl;
    out << "  " << m_argv[0] << " --remove <config>" << endl;
    out << "Modify configuration:" << endl;
    out << "  " << m_argv[0] << " --configure <options for configuration> <config> [<source> ...]" << endl;
    out << "  " << m_argv[0] << " --migrate <config>" << endl;
    if (full) {
        out << endl <<
            "Options:" << endl <<
            "--sync|-s <mode>" << endl <<
            "--sync|-s ?" << endl <<
            "  Temporarily synchronize the active sources in that mode. Useful" << endl <<
            "  for a \"refresh-from-server\" or \"refresh-from-client\" sync which" << endl <<
            "  clears all data at one end and copies all items from the other." << endl <<
            "" << endl <<
            "--print-servers|--print-configs|--print-peers" << endl <<
            "  Prints the names of all configured peers to stdout." << endl <<
            "" << endl <<
            "--print-config|-p" << endl <<
            "  Prints the complete configuration for the selected peer" << endl <<
            "  to stdout, including up-to-date comments for all properties. The" << endl <<
            "  format is the normal .ini format with source configurations in" << endl <<
            "  different sections introduced with [<source>] lines. Can be combined" << endl <<
            "  with --sync-property and --source-property to modify the configuration" << endl <<
            "  on-the-fly. When one or more sources are listed after the <config>" << endl <<
            "  name on the command line, then only the configs of those sources are" << endl <<
            "  printed. Using --quiet suppresses the comments for each property." << endl <<
            "  When setting a --template, then the reference configuration for" << endl <<
            "  that peer is printed instead of an existing configuration." << endl <<
            "" << endl <<
            "--print-sessions" << endl <<
            "  Prints a list of all previous log directories. Unless --quiet is used, each" << endl <<
            "  file name is followed by the original sync report." << endl <<
            "" << endl <<
            "--configure|-c" << endl <<
            "  Modify the configuration files for the selected peer. If no such" << endl <<
            "  configuration exists, then a new one is created using one of the" << endl <<
            "  template configurations (see --template option). When creating" << endl <<
            "  a new configuration only the active sources will be set to active" << endl <<
            "  in the new configuration, i.e. \"syncevolution -c scheduleworld addressbook\"" << endl <<
            "  followed by \"syncevolution scheduleworld\" will only synchronize the" << endl <<
            "  address book. The other sources are created in a disabled state." << endl <<
            "  When modifying an existing configuration and sources are specified," << endl <<
            "  then the source properties of only those sources are modified." << endl <<
            "" << endl <<
            "--migrate" << endl <<
            "  In older SyncEvolution releases a different layout of configuration files" << endl <<
            "  was used. Using --migrate will automatically migrate to the new" << endl <<
            "  layout and rename the <config> into <config>.old to prevent accidental use" << endl <<
            "  of the old configuration. WARNING: old SyncEvolution releases cannot" << endl <<
            "  use the new configuration!" << endl <<
            "" << endl <<
            "  The switch can also be used to migrate a configuration in the current" << endl <<
            "  configuration directory: this preserves all property values, discards" << endl <<
            "  obsolete properties and sets all comments exactly as if the configuration" << endl <<
            "  had been created from scratch. WARNING: custom comments in the" << endl <<
            "  configuration are not preserved." << endl <<
            "" << endl <<
            "--restore" << endl <<
            "  Restores the data of the selected sources to the state from before or after the" << endl <<
            "  selected synchronization. The synchronization is selected via its log directory" << endl <<
            "  (see --print-sessions). Other directories can also be given as long as" << endl <<
            "  they contain database dumps in the format created by SyncEvolution." << endl <<
            "  The output includes information about the changes made during the" << endl <<
            "  restore, both in terms of item changes and content changes (which is" << endl <<
            "  not always the same, see manual for details). This output can be suppressed" << endl <<
            "  with --quiet." << endl <<
            "  In combination with --dry-run, the changes to local data are only simulated." << endl <<
            "  This can be used to check that --restore will not remove valuable information." << endl <<
            "" << endl <<
            "--remove" << endl <<
            "  Deletes the configuration. If the <config> refers to a specific" << endl <<
            "  peer, only that peer's configuration is removed. If it refers to" << endl <<
            "  a context, that context and all peers inside it are removed." << endl <<
            "  Note that there is no confirmation question. Neither local data" << endl <<
            "  referenced by the configuration nor the content of log dirs are" << endl <<
            "  deleted." << endl <<
            "" << endl <<
            "--sync-property|-y <property>=<value>" << endl <<
            "--sync-property|-y ?" << endl <<
            "--sync-property|-y <property>=?" << endl <<
            "  Overrides a source-independent configuration property for the" << endl <<
            "  current synchronization run or permanently when --configure is used" << endl <<
            "  to update the configuration. Can be used multiple times.  Specifying" << endl <<
            "  an unused property will trigger an error message." << endl <<
            "" << endl <<
            "  When using the configuration layout introduced with 1.0, some of the" << endl <<
            "  sync properties are shared between peers, for example the directory" << endl <<
            "  where sessions are logged. Permanently changing such a shared" << endl <<
            "  property for one peer will automatically update the property for all" << endl <<
            "  other peers in the same context because the property is stored in a" << endl <<
            "  shared config file." << endl <<
            "" << endl <<
            "--source-property|-z <property>=<value>" << endl <<
            "--source-property|-z ?" << endl <<
            "--source-property|-z <property>=?" << endl <<
            "  Same as --sync-property, but applies to the configuration of all active" << endl <<
            "  sources. \"--sync <mode>\" is a shortcut for \"--source-property sync=<mode>\"." << endl <<
            "" << endl <<
            "--template|-l <peer name>|default|?|?<device>" << endl <<
            "  Can be used to select from one of the built-in default configurations" << endl <<
            "  for known SyncML peers. Defaults to the <config> name, so --template" << endl <<
            "  only has to be specified when creating multiple different configurations" << endl <<
            "  for the same peer, or when using a template that is named differently" << endl <<
            "  than the peer. \"default\" is an alias for \"scheduleworld\" and can be" << endl <<
            "  used as the starting point for servers which do not have a built-in" << endl <<
            "  template." << endl <<
            "" << endl <<
            "  Each template contains a pseudo-random device ID. Therefore setting the" << endl <<
            "  \"deviceId\" sync property is only necessary when manually recreating a" << endl <<
            "  configuration or when a more descriptive name is desired." << endl <<
            "" << endl <<
            "  The available templates for different known SyncML servers are listed when" << endl <<
            "  using a single question mark instead of template name. When using the" << endl <<
            "  ?<device> format, a fuzzy search for a template that might be" << endl <<
            "  suitable for talking to such a device is done. The matching works best" << endl <<
            "  when using <device> = <Manufacturer>_<Model>. If you don't know the" << endl <<
            "  manufacturer, you can just keep it as empty. The output in this mode" << endl <<
            "  gives the template name followed by a short description and a rating how well" << endl <<
            "  the template matches the device (higher is better)." << endl <<
            "" << endl <<
            "--status|-t" << endl <<
            "  The changes made to local data since the last synchronization are" << endl <<
            "  shown without starting a new one. This can be used to see in advance" << endl <<
            "  whether the local data needs to be synchronized with the peer." << endl <<
            "" << endl <<
            "--quiet|-q" << endl <<
            "  Suppresses most of the normal output during a synchronization. The" << endl <<
            "  log file still contains all the information." << endl <<
            "" << endl <<
            "--keyring|-k" << endl <<
            "  Save or retrieve passwords from the GNOME keyring when modifying the" << endl <<
            "  configuration or running a synchronization. Note that using this option" << endl <<
            "  applies to *all* passwords in a configuration, so setting a single" << endl <<
            "  password as follows moves the other passwords into the keyring, if" << endl <<
            "  they were not stored there already:" << endl <<
            "     --keyring --configure --sync-property proxyPassword=foo" << endl <<
            "" << endl <<
            "  When passwords were stored in the keyring, their value is set to '-'" << endl <<
            "  in the configuration. This means that when running a synchronization" << endl <<
            "  without the --keyring argument, the password has to be entered" << endl <<
            "  interactively. The --print-config output always shows '-' instead of" << endl <<
            "  retrieving the password from the keyring." << endl <<
            "" << endl <<
            "--help|-h" << endl <<
            "  Prints usage information." << endl <<
            "" << endl <<
            "--version" << endl <<
            "  Prints the SyncEvolution version." << endl;
    }

    if (error != "") {
        out << endl << "ERROR: " << error << endl;
    }
    if (param != "") {
        out << "INFO: use '" << param << (param[param.size() - 1] == '=' ? "" : " ") <<
            "?' to get a list of valid parameters" << endl;
    }
}

SyncContext* Cmdline::createSyncClient() {
    return new SyncContext(m_server, true);
}

#ifdef ENABLE_UNIT_TESTS

/** simple line-by-line diff */
static string diffStrings(const string &lhs, const string &rhs)
{
    ostringstream res;

    typedef boost::split_iterator<string::const_iterator> string_split_iterator;
    string_split_iterator lit =
        boost::make_split_iterator(lhs, boost::first_finder("\n", boost::is_iequal()));
    string_split_iterator rit =
        boost::make_split_iterator(rhs, boost::first_finder("\n", boost::is_iequal()));
    while (lit != string_split_iterator() &&
           rit != string_split_iterator()) {
        if (*lit != *rit) {
            res << "< " << *lit << endl;
            res << "> " << *rit << endl;
        }
        ++lit;
        ++rit;
    }

    while (lit != string_split_iterator()) {
        res << "< " << *lit << endl;
        ++lit;
    }

    while (rit != string_split_iterator()) {
        res << "> " << *rit << endl;
        ++rit;
    }

    return res.str();
}

# define CPPUNIT_ASSERT_EQUAL_DIFF( expected, actual )      \
    do { \
        string expected_ = (expected);                                  \
        string actual_ = (actual);                                      \
        if (expected_ != actual_) {                                     \
            CPPUNIT_NS::Message cpputMsg_(string("expected:\n") +       \
                                          expected_);                   \
            cpputMsg_.addDetail(string("actual:\n") +                   \
                                actual_);                               \
            cpputMsg_.addDetail(string("diff:\n") +                     \
                                diffStrings(expected_, actual_));       \
            CPPUNIT_NS::Asserter::fail( cpputMsg_,                      \
                                        CPPUNIT_SOURCELINE() );         \
        } \
    } while ( false )

// returns last line, including trailing line break, empty if input is empty
static string lastLine(const string &buffer)
{
    if (buffer.size() < 2) {
        return buffer;
    }

    size_t line = buffer.rfind("\n", buffer.size() - 2);
    if (line == buffer.npos) {
        return buffer;
    }

    return buffer.substr(line + 1);
}

// true if <word> =
static bool isPropAssignment(const string &buffer) {
    size_t start = 0;
    while (start < buffer.size() &&
           !isspace(buffer[start])) {
        start++;
    }
    if (start + 3 <= buffer.size() &&
        buffer.substr(start, 3) == " = ") {
        return true;
    } else {
        return false;
    }
}

// remove pure comment lines from buffer,
// also empty lines,
// also defaultPeer (because reference properties do not include global props)
static string filterConfig(const string &buffer)
{
    ostringstream res;

    typedef boost::split_iterator<string::const_iterator> string_split_iterator;
    for (string_split_iterator it =
             boost::make_split_iterator(buffer, boost::first_finder("\n", boost::is_iequal()));
         it != string_split_iterator();
         ++it) {
        string line = boost::copy_range<string>(*it);
        if (!line.empty() &&
            line.find("defaultPeer =") == line.npos &&
            (!boost::starts_with(line, "# ") ||
             isPropAssignment(line.substr(2)))) {
            res << line << endl;
        }
    }

    return res.str();
}

static string injectValues(const string &buffer)
{
    string res = buffer;

    // username/password not set in templates, only in configs created via
    // the command line
    boost::replace_first(res,
                         "# username = ",
                         "username = your SyncML server account name");
    boost::replace_first(res,
                         "# password = ",
                         "password = your SyncML server password");
    return res;
}

// remove lines indented with spaces
static string filterIndented(const string &buffer)
{
    ostringstream res;
    bool first = true;

    typedef boost::split_iterator<string::const_iterator> string_split_iterator;
    for (string_split_iterator it =
             boost::make_split_iterator(buffer, boost::first_finder("\n", boost::is_iequal()));
         it != string_split_iterator();
         ++it) {
        if (!boost::starts_with(*it, " ")) {
            if (!first) {
                res << endl;
            } else {
                first = false;
            }
            res << *it;
        }
    }

    return res.str();
}

// sort lines by file, preserving order inside each line
static void sortConfig(string &config)
{
    // file name, line number, property
    typedef pair<string, pair<int, string> > line_t;
    vector<line_t> lines;
    typedef boost::split_iterator<string::iterator> string_split_iterator;
    int linenr = 0;
    for (string_split_iterator it =
             boost::make_split_iterator(config, boost::first_finder("\n", boost::is_iequal()));
         it != string_split_iterator();
         ++it, ++linenr) {
        string line(it->begin(), it->end());
        if (line.empty()) {
            continue;
        }

        size_t colon = line.find(':');
        string prefix = line.substr(0, colon);
        lines.push_back(make_pair(prefix, make_pair(linenr, line.substr(colon))));
    }

    // stable sort because of line number
    sort(lines.begin(), lines.end());

    size_t len = config.size();
    config.resize(0);
    config.reserve(len);
    BOOST_FOREACH(const line_t &line, lines) {
        config += line.first;
        config += line.second.second;
        config += "\n";
    }
}

// convert the internal config dump to .ini style (--print-config)
static string internalToIni(const string &config)
{
    ostringstream res;

    string section;
    typedef boost::split_iterator<string::const_iterator> string_split_iterator;
    for (string_split_iterator it =
             boost::make_split_iterator(config, boost::first_finder("\n", boost::is_iequal()));
         it != string_split_iterator();
         ++it) {
        string line(it->begin(), it->end());
        if (line.empty()) {
            continue;
        }

        size_t colon = line.find(':');
        string prefix = line.substr(0, colon);

        // internal values are not part of the --print-config output
        if (boost::contains(prefix, ".internal.ini") ||
            boost::contains(line, "= internal value")) {
            continue;
        }

        // --print-config also doesn't duplicate the "type" property
        // => remove the shared property
        if (boost::contains(line, ":type = ") &&
            boost::starts_with(line, "sources/")) {
            continue;
        }

        // sources/<name>/config.ini or
        // spds/sources/<name>/config.ini
        size_t endslash = prefix.rfind('/');
        if (endslash != line.npos && endslash > 1) {
            size_t slash = prefix.rfind('/', endslash - 1);
            if (slash != line.npos) {
                string newsource = prefix.substr(slash + 1, endslash - slash - 1);
                if (newsource != section &&
                    prefix.find("/sources/") != prefix.npos &&
                    newsource != "syncml") {
                    res << "[" << newsource << "]" << endl;
                    section = newsource;
                }
            }
        }
        string assignment = line.substr(colon + 1);
        // substitude aliases with generic values
        boost::replace_first(assignment, "= F", "= 0");
        boost::replace_first(assignment, "= T", "= 1");
        boost::replace_first(assignment, "= syncml:auth-md5", "= md5");
        boost::replace_first(assignment, "= syncml:auth-basix", "= basic");
        res << assignment << endl;
    }

    return res.str();
}


/**
 * Testing is based on a text representation of a directory
 * hierarchy where each line is of the format
 * <file path>:<line in file>
 *
 * The order of files is alphabetical, of lines in the file as
 * in the file. Lines in the file without line break cannot
 * be represented.
 *
 * The root of the hierarchy is not part of the representation
 * itself.
 */
class CmdlineTest : public CppUnit::TestFixture {
    CPPUNIT_TEST_SUITE(CmdlineTest);
    CPPUNIT_TEST(testFramework);
    CPPUNIT_TEST(testSetupScheduleWorld);
    CPPUNIT_TEST(testSetupDefault);
    CPPUNIT_TEST(testSetupRenamed);
    CPPUNIT_TEST(testSetupFunambol);
    CPPUNIT_TEST(testSetupSynthesis);
    CPPUNIT_TEST(testPrintServers);
    CPPUNIT_TEST(testPrintConfig);
    CPPUNIT_TEST(testPrintFileTemplates);
    CPPUNIT_TEST(testTemplate);
    CPPUNIT_TEST(testMatchTemplate);
    CPPUNIT_TEST(testAddSource);
    CPPUNIT_TEST(testSync);
    CPPUNIT_TEST(testConfigure);
    CPPUNIT_TEST(testOldConfigure);
    CPPUNIT_TEST(testListSources);
    CPPUNIT_TEST(testMigrate);
    CPPUNIT_TEST_SUITE_END();
    
public:
    CmdlineTest() :
        m_testDir("CmdlineTest"),
        // properties sorted by the order in which they are defined
        // in the sync and sync source property registry
        m_scheduleWorldConfig("peers/scheduleworld/.internal.ini:# HashCode = 0\n"
                              "peers/scheduleworld/.internal.ini:# ConfigDate = \n"
                              "peers/scheduleworld/.internal.ini:# lastNonce = \n"
                              "peers/scheduleworld/.internal.ini:# deviceData = \n"
                              "peers/scheduleworld/config.ini:syncURL = http://sync.scheduleworld.com/funambol/ds\n"
                              "peers/scheduleworld/config.ini:username = your SyncML server account name\n"
                              "peers/scheduleworld/config.ini:password = your SyncML server password\n"
                              "config.ini:# logdir = \n"
                              "peers/scheduleworld/config.ini:# loglevel = 0\n"
                              "peers/scheduleworld/config.ini:# printChanges = 1\n"
                              "config.ini:# maxlogdirs = 10\n"
                              "peers/scheduleworld/config.ini:# preventSlowSync = 0\n"
                              "peers/scheduleworld/config.ini:# useProxy = 0\n"
                              "peers/scheduleworld/config.ini:# proxyHost = \n"
                              "peers/scheduleworld/config.ini:# proxyUsername = \n"
                              "peers/scheduleworld/config.ini:# proxyPassword = \n"
                              "peers/scheduleworld/config.ini:# clientAuthType = md5\n"
                              "peers/scheduleworld/config.ini:# RetryDuration = 300\n"
                              "peers/scheduleworld/config.ini:# RetryInterval = 60\n"
                              "peers/scheduleworld/config.ini:# remoteIdentifier = \n"
                              "peers/scheduleworld/config.ini:# PeerIsClient = 0\n"
                              "peers/scheduleworld/config.ini:# PeerName = \n"
                              "config.ini:deviceId = fixed-devid\n" /* this is not the default! */
                              "peers/scheduleworld/config.ini:# remoteDeviceId = \n"
                              "peers/scheduleworld/config.ini:# enableWBXML = 1\n"
                              "peers/scheduleworld/config.ini:# maxMsgSize = 20000\n"
                              "peers/scheduleworld/config.ini:# maxObjSize = 4000000\n"
                              "peers/scheduleworld/config.ini:# enableCompression = 0\n"
                              "peers/scheduleworld/config.ini:# SSLServerCertificates = \n"
                              "peers/scheduleworld/config.ini:# SSLVerifyServer = 1\n"
                              "peers/scheduleworld/config.ini:# SSLVerifyHost = 1\n"
                              "peers/scheduleworld/config.ini:WebURL = http://www.scheduleworld.com\n"
                              "peers/scheduleworld/config.ini:# IconURI = \n"
                              "peers/scheduleworld/config.ini:ConsumerReady = 1\n"

                              "peers/scheduleworld/sources/addressbook/.internal.ini:# adminData = \n"
                              "peers/scheduleworld/sources/addressbook/.internal.ini:# synthesisID = 0\n"
                              "peers/scheduleworld/sources/addressbook/config.ini:sync = two-way\n"
                              "sources/addressbook/config.ini:type = addressbook:text/vcard\n"
                              "peers/scheduleworld/sources/addressbook/config.ini:type = addressbook:text/vcard\n"
                              "sources/addressbook/config.ini:# evolutionsource = \n"
                              "peers/scheduleworld/sources/addressbook/config.ini:uri = card3\n"
                              "sources/addressbook/config.ini:# evolutionuser = \n"
                              "sources/addressbook/config.ini:# evolutionpassword = \n"

                              "peers/scheduleworld/sources/calendar/.internal.ini:# adminData = \n"
                              "peers/scheduleworld/sources/calendar/.internal.ini:# synthesisID = 0\n"
                              "peers/scheduleworld/sources/calendar/config.ini:sync = two-way\n"
                              "sources/calendar/config.ini:type = calendar\n"
                              "peers/scheduleworld/sources/calendar/config.ini:type = calendar\n"
                              "sources/calendar/config.ini:# evolutionsource = \n"
                              "peers/scheduleworld/sources/calendar/config.ini:uri = cal2\n"
                              "sources/calendar/config.ini:# evolutionuser = \n"
                              "sources/calendar/config.ini:# evolutionpassword = \n"

                              "peers/scheduleworld/sources/memo/.internal.ini:# adminData = \n"
                              "peers/scheduleworld/sources/memo/.internal.ini:# synthesisID = 0\n"
                              "peers/scheduleworld/sources/memo/config.ini:sync = two-way\n"
                              "sources/memo/config.ini:type = memo\n"
                              "peers/scheduleworld/sources/memo/config.ini:type = memo\n"
                              "sources/memo/config.ini:# evolutionsource = \n"
                              "peers/scheduleworld/sources/memo/config.ini:uri = note\n"
                              "sources/memo/config.ini:# evolutionuser = \n"
                              "sources/memo/config.ini:# evolutionpassword = \n"

                              "peers/scheduleworld/sources/todo/.internal.ini:# adminData = \n"
                              "peers/scheduleworld/sources/todo/.internal.ini:# synthesisID = 0\n"
                              "peers/scheduleworld/sources/todo/config.ini:sync = two-way\n"
                              "sources/todo/config.ini:type = todo\n"
                              "peers/scheduleworld/sources/todo/config.ini:type = todo\n"
                              "sources/todo/config.ini:# evolutionsource = \n"
                              "peers/scheduleworld/sources/todo/config.ini:uri = task2\n"
                              "sources/todo/config.ini:# evolutionuser = \n"
                              "sources/todo/config.ini:# evolutionpassword = ")
    {
#ifdef ENABLE_LIBSOUP
        // path to SSL certificates has to be set only for libsoup
        boost::replace_first(m_scheduleWorldConfig,
                             "SSLServerCertificates = ",
                             "SSLServerCertificates = /etc/ssl/certs/ca-certificates.crt:/etc/pki/tls/certs/ca-bundle.crt:/usr/share/ssl/certs/ca-bundle.crt");
#endif
    }

protected:

    /** verify that createFiles/scanFiles themselves work */
    void testFramework() {
        const string root(m_testDir);
        const string content("baz:line\n"
                             "caz/subdir:booh\n"
                             "caz/subdir2/sub:# comment\n"
                             "caz/subdir2/sub:# foo = bar\n"
                             "caz/subdir2/sub:# empty = \n"
                             "caz/subdir2/sub:# another comment\n"
                             "foo:bar1\n"
                             "foo:\n"
                             "foo: \n"
                             "foo:bar2\n");
        const string filtered("baz:line\n"
                              "caz/subdir:booh\n"
                              "caz/subdir2/sub:# foo = bar\n"
                              "caz/subdir2/sub:# empty = \n"
                              "foo:bar1\n"
                              "foo: \n"
                              "foo:bar2\n");
        createFiles(root, content);
        string res = scanFiles(root);
        CPPUNIT_ASSERT_EQUAL_DIFF(filtered, res);
    }

    void removeRandomUUID(string &buffer) {
        string uuidstr = "deviceId = sc-pim-";
        size_t uuid = buffer.find(uuidstr);
        CPPUNIT_ASSERT(uuid != buffer.npos);
        size_t end = buffer.find("\n", uuid + uuidstr.size());
        CPPUNIT_ASSERT(end != buffer.npos);
        buffer.replace(uuid, end - uuid, "deviceId = fixed-devid");
    }

    /** create new configurations */
    void testSetupScheduleWorld() { doSetupScheduleWorld(false); }
    void doSetupScheduleWorld(bool shared) {
        string root;
        ScopedEnvChange templates("SYNCEVOLUTION_TEMPLATE_DIR", "/dev/null");
        ScopedEnvChange xdg("XDG_CONFIG_HOME", m_testDir);
        ScopedEnvChange home("HOME", m_testDir);

        root = m_testDir;
        root += "/syncevolution/default";
        string peer;
        if (shared) {
            peer = root + "/peers/scheduleworld";
        } else {
            peer = root;
        }

        {
            rm_r(peer);
            TestCmdline cmdline("--configure",
                                "--sync-property", "proxyHost = proxy",
                                "scheduleworld",
                                "addressbook",
                                NULL);
            cmdline.doit();
            string res = scanFiles(root);
            removeRandomUUID(res);
            string expected = ScheduleWorldConfig();
            sortConfig(expected);
            boost::replace_first(expected,
                                 "# proxyHost = ",
                                 "proxyHost = proxy");
            boost::replace_all(expected,
                               "sync = two-way",
                               "sync = disabled");
            boost::replace_first(expected,
                                 "addressbook/config.ini:sync = disabled",
                                 "addressbook/config.ini:sync = two-way");
            CPPUNIT_ASSERT_EQUAL_DIFF(expected, res);
        }

        {
            rm_r(peer);
            TestCmdline cmdline("--configure",
                                "--sync-property", "deviceID = fixed-devid",
                                "scheduleworld",
                                NULL);
            cmdline.doit();
            string res = scanFiles(root);
            string expected = ScheduleWorldConfig();
            sortConfig(expected);
            CPPUNIT_ASSERT_EQUAL_DIFF(expected, res);
        }
    }

    void testSetupDefault() {
        string root;
        ScopedEnvChange templates("SYNCEVOLUTION_TEMPLATE_DIR", "/dev/null");
        ScopedEnvChange xdg("XDG_CONFIG_HOME", m_testDir);
        ScopedEnvChange home("HOME", m_testDir);

        root = m_testDir;
        root += "/syncevolution/default";
        rm_r(root);
        TestCmdline cmdline("--configure",
                            "--template", "default",
                            "--sync-property", "deviceID = fixed-devid",
                            "some-other-server",
                            NULL);
        cmdline.doit();
        string res = scanFiles(root, "some-other-server");
        string expected = ScheduleWorldConfig();
        sortConfig(expected);
        boost::replace_all(expected, "/scheduleworld/", "/some-other-server/");
        CPPUNIT_ASSERT_EQUAL_DIFF(expected, res);
    }
    void testSetupRenamed() {
        string root;
        ScopedEnvChange templates("SYNCEVOLUTION_TEMPLATE_DIR", "/dev/null");
        ScopedEnvChange xdg("XDG_CONFIG_HOME", m_testDir);
        ScopedEnvChange home("HOME", m_testDir);

        root = m_testDir;
        root += "/syncevolution/default";
        rm_r(root);
        TestCmdline cmdline("--configure",
                            "--template", "scheduleworld",
                            "--sync-property", "deviceID = fixed-devid",
                            "scheduleworld2",
                            NULL);
        cmdline.doit();
        string res = scanFiles(root, "scheduleworld2");
        string expected = ScheduleWorldConfig();
        sortConfig(expected);
        boost::replace_all(expected, "/scheduleworld/", "/scheduleworld2/");
        CPPUNIT_ASSERT_EQUAL_DIFF(expected, res);
    }

    void testSetupFunambol() { doSetupFunambol(false); }
    void doSetupFunambol(bool shared) {
        string root;
        ScopedEnvChange templates("SYNCEVOLUTION_TEMPLATE_DIR", "/dev/null");
        ScopedEnvChange xdg("XDG_CONFIG_HOME", m_testDir);
        ScopedEnvChange home("HOME", m_testDir);

        root = m_testDir;
        root += "/syncevolution/default";
        string peer;
        if (shared) {
            peer = root + "/peers/funambol";
        } else {
            peer = root;
        }

        rm_r(peer);
        const char * const argv_fixed[] = {
                "--configure",
                "--sync-property", "deviceID = fixed-devid",
                // templates are case-insensitive
                "FunamBOL",
                NULL
        }, * const argv_shared[] = {
            "--configure",
            "FunamBOL",
            NULL
        };
        TestCmdline cmdline(shared ? argv_shared : argv_fixed);
        cmdline.doit();
        string res = scanFiles(root, "funambol");
        string expected = FunambolConfig();
        sortConfig(expected);
        CPPUNIT_ASSERT_EQUAL_DIFF(expected, res);
    }

    void testSetupSynthesis() { doSetupSynthesis(false); }
    void doSetupSynthesis(bool shared) {
        string root;
        ScopedEnvChange templates("SYNCEVOLUTION_TEMPLATE_DIR", "/dev/null");
        ScopedEnvChange xdg("XDG_CONFIG_HOME", m_testDir);
        ScopedEnvChange home("HOME", m_testDir);

        root = m_testDir;
        root += "/syncevolution/default";
        string peer;
        if (shared) {
            peer = root + "/peers/synthesis";
        } else {
            peer = root;
        }
        rm_r(peer);
        const char * const argv_fixed[] = {
                "--configure",
                "--sync-property", "deviceID = fixed-devid",
                "synthesis",
                NULL
        }, * const argv_shared[] = {
            "--configure",
            "synthesis",
            NULL
        };
        TestCmdline cmdline(shared ? argv_shared : argv_fixed);
        cmdline.doit();
        string res = scanFiles(root, "synthesis");
        string expected = SynthesisConfig();
        sortConfig(expected);
        CPPUNIT_ASSERT_EQUAL_DIFF(expected, res);
    }

    void testTemplate() {
        TestCmdline failure("--template", NULL);
        CPPUNIT_ASSERT(!failure.m_cmdline->parse());
        CPPUNIT_ASSERT_EQUAL_DIFF("", failure.m_out.str());
        CPPUNIT_ASSERT_EQUAL(string("ERROR: missing parameter for '--template'\n"), lastLine(failure.m_err.str()));

        TestCmdline help("--template", "? ", NULL);
        help.doit();
        CPPUNIT_ASSERT_EQUAL_DIFF("Available configuration templates:\n"
                                  "   Funambol = http://my.funambol.com\n"
                                  "   Google = http://m.google.com/sync\n"
                                  "   Goosync = http://www.goosync.com/\n"
                                  "   Memotoo = http://www.memotoo.com\n"
                                  "   Mobical = http://www.mobical.net\n"
                                  "   Oracle = http://www.oracle.com/technology/products/beehive/index.html\n"
                                  "   ScheduleWorld = http://www.scheduleworld.com\n"
                                  "   SyncEvolution = http://www.syncevolution.org\n"
                                  "   Synthesis = http://www.synthesis.ch\n"
                                  "   ZYB = http://www.zyb.com\n",
                                  help.m_out.str());
        CPPUNIT_ASSERT_EQUAL_DIFF("", help.m_err.str());
    }

    void testMatchTemplate() {
        ScopedEnvChange templates("SYNCEVOLUTION_TEMPLATE_DIR", "testcases/templates");

        TestCmdline help1("--template", "?nokia_7210c", NULL);
        help1.doit();
        CPPUNIT_ASSERT_EQUAL_DIFF("Available configuration templates:\n"
                "   Nokia_7210c = Template for Nokia 7210c phone and as default template for all Nokia phones    5\n"
                "   SyncEvolutionClient = SyncEvolution server side template    2\n"
                "   ServerDefault = server side default template    1\n",
                help1.m_out.str());
        CPPUNIT_ASSERT_EQUAL_DIFF("", help1.m_err.str());
        TestCmdline help2("--template", "?nokia", NULL);
        help2.doit();
        CPPUNIT_ASSERT_EQUAL_DIFF("Available configuration templates:\n"
                "   Nokia_7210c = Template for Nokia 7210c phone and as default template for all Nokia phones    5\n"
                "   SyncEvolutionClient = SyncEvolution server side template    2\n"
                "   ServerDefault = server side default template    1\n",
                help2.m_out.str());
        CPPUNIT_ASSERT_EQUAL_DIFF("", help2.m_err.str());
        TestCmdline help3("--template", "?7210c", NULL);
        help3.doit();
        CPPUNIT_ASSERT_EQUAL_DIFF("Available configuration templates:\n"
                "   Nokia_7210c = Template for Nokia 7210c phone and as default template for all Nokia phones    3\n"
                "   ServerDefault = server side default template    1\n"
                "   SyncEvolutionClient = SyncEvolution server side template    1\n",
                help3.m_out.str());
        CPPUNIT_ASSERT_EQUAL_DIFF("", help3.m_err.str());
        TestCmdline help4("--template", "?syncevolution", NULL);
        help4.doit();
        CPPUNIT_ASSERT_EQUAL_DIFF("Available configuration templates:\n"
                "   SyncEvolutionClient = SyncEvolution server side template    5\n"
                "   Nokia_7210c = Template for Nokia 7210c phone and as default template for all Nokia phones    2\n"
                "   ServerDefault = server side default template    2\n",
                help4.m_out.str());
        CPPUNIT_ASSERT_EQUAL_DIFF("", help4.m_err.str());
    }

    void testPrintServers() {
        ScopedEnvChange templates("SYNCEVOLUTION_TEMPLATE_DIR", "/dev/null");
        ScopedEnvChange xdg("XDG_CONFIG_HOME", m_testDir);
        ScopedEnvChange home("HOME", m_testDir);

        rm_r(m_testDir);
        doSetupScheduleWorld(false);
        doSetupSynthesis(true);
        doSetupFunambol(true);

        TestCmdline cmdline("--print-servers", NULL);
        cmdline.doit();
        CPPUNIT_ASSERT_EQUAL_DIFF("Configured servers:\n"
                                  "   funambol = CmdlineTest/syncevolution/default/peers/funambol\n"
                                  "   scheduleworld = CmdlineTest/syncevolution/default/peers/scheduleworld\n"
                                  "   synthesis = CmdlineTest/syncevolution/default/peers/synthesis\n",
                                  cmdline.m_out.str());
        CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
    }

    void testPrintConfig() {
        ScopedEnvChange templates("SYNCEVOLUTION_TEMPLATE_DIR", "/dev/null");
        ScopedEnvChange xdg("XDG_CONFIG_HOME", m_testDir);
        ScopedEnvChange home("HOME", m_testDir);

        rm_r(m_testDir);
        testSetupFunambol();

        {
            TestCmdline failure("--print-config", NULL);
            CPPUNIT_ASSERT(failure.m_cmdline->parse());
            CPPUNIT_ASSERT(!failure.m_cmdline->run());
            CPPUNIT_ASSERT_EQUAL_DIFF("", failure.m_out.str());
            CPPUNIT_ASSERT_EQUAL(string("ERROR: --print-config requires either a --template or a server name.\n"),
                                 lastLine(failure.m_err.str()));
        }

        {
            TestCmdline failure("--print-config", "foo", NULL);
            CPPUNIT_ASSERT(failure.m_cmdline->parse());
            CPPUNIT_ASSERT(!failure.m_cmdline->run());
            CPPUNIT_ASSERT_EQUAL_DIFF("", failure.m_out.str());
            CPPUNIT_ASSERT_EQUAL(string("ERROR: server 'foo' has not been configured yet.\n"),
                                 lastLine(failure.m_err.str()));
        }

        {
            TestCmdline failure("--print-config", "--template", "foo", NULL);
            CPPUNIT_ASSERT(failure.m_cmdline->parse());
            CPPUNIT_ASSERT(!failure.m_cmdline->run());
            CPPUNIT_ASSERT_EQUAL_DIFF("", failure.m_out.str());
            CPPUNIT_ASSERT_EQUAL(string("ERROR: no configuration template for 'foo' available.\n"),
                                 lastLine(failure.m_err.str()));
        }

        {
            TestCmdline cmdline("--print-config", "--template", "scheduleworld", NULL);
            cmdline.doit();
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
            string actual = cmdline.m_out.str();
            // deviceId must be the one from Funambol
            CPPUNIT_ASSERT(boost::contains(actual, "deviceId = fixed-devid"));
            string filtered = injectValues(filterConfig(actual));
            CPPUNIT_ASSERT_EQUAL_DIFF(filterConfig(internalToIni(ScheduleWorldConfig())),
                                      filtered);
            // there should have been comments
            CPPUNIT_ASSERT(actual.size() > filtered.size());
        }

        {
            TestCmdline cmdline("--print-config", "--template", "Default", NULL);
            cmdline.doit();
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
            string actual = injectValues(filterConfig(cmdline.m_out.str()));
            CPPUNIT_ASSERT(boost::contains(actual, "deviceId = fixed-devid"));
            CPPUNIT_ASSERT_EQUAL_DIFF(filterConfig(internalToIni(ScheduleWorldConfig())),
                                      actual);
        }

        {
            TestCmdline cmdline("--print-config", "funambol", NULL);
            cmdline.doit();
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
            CPPUNIT_ASSERT_EQUAL_DIFF(filterConfig(internalToIni(FunambolConfig())),
                                      injectValues(filterConfig(cmdline.m_out.str())));
        }

        {
            // override context and template properties
            TestCmdline cmdline("--print-config", "--template", "scheduleworld",
                                "--sync-property", "syncURL=foo",
                                "--source-property", "evolutionsource=Personal",
                                "--source-property", "sync=disabled",
                                NULL);
            cmdline.doit();
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
            string expected = filterConfig(internalToIni(ScheduleWorldConfig()));
            boost::replace_first(expected,
                                 "syncURL = http://sync.scheduleworld.com/funambol/ds",
                                 "syncURL = foo");
            boost::replace_all(expected,
                               "# evolutionsource = ",
                               "evolutionsource = Personal");
            boost::replace_all(expected,
                               "sync = two-way",
                               "sync = disabled");
            string actual = injectValues(filterConfig(cmdline.m_out.str()));
            CPPUNIT_ASSERT(boost::contains(actual, "deviceId = fixed-devid"));
            CPPUNIT_ASSERT_EQUAL_DIFF(expected,
                                      actual);
        }

        {
            TestCmdline cmdline("--print-config", "--quiet",
                                "--template", "scheduleworld",
                                "funambol",
                                NULL);
            cmdline.doit();
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
            string actual = cmdline.m_out.str();
            CPPUNIT_ASSERT(boost::contains(actual, "deviceId = fixed-devid"));
            CPPUNIT_ASSERT_EQUAL_DIFF(internalToIni(ScheduleWorldConfig()),
                                      injectValues(filterConfig(actual)));
        }

        {
            // change shared source properties, then check template again
            TestCmdline cmdline("--configure",
                                "--source-property", "evolutionsource=Personal",
                                "funambol",
                                NULL);
            cmdline.doit();
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
        }
        {
            TestCmdline cmdline("--print-config", "--quiet",
                                "--template", "scheduleworld",
                                "funambol",
                                NULL);
            cmdline.doit();
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
            string expected = filterConfig(internalToIni(ScheduleWorldConfig()));
            // from modified Funambol config
            boost::replace_all(expected,
                               "# evolutionsource = ",
                               "evolutionsource = Personal");
            string actual = injectValues(filterConfig(cmdline.m_out.str()));
            CPPUNIT_ASSERT(boost::contains(actual, "deviceId = fixed-devid"));
            CPPUNIT_ASSERT_EQUAL_DIFF(expected,
                                      actual);
        }
    }

    void testPrintFileTemplates() {
        // use local copy of templates in build dir (no need to install)
        ScopedEnvChange templates("SYNCEVOLUTION_TEMPLATE_DIR", "./templates");
        ScopedEnvChange xdg("XDG_CONFIG_HOME", m_testDir);
        ScopedEnvChange home("HOME", m_testDir);

        rm_r(m_testDir);
        testSetupFunambol();

        {
            TestCmdline cmdline("--print-config", "--template", "scheduleworld", NULL);
            cmdline.doit();
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
            string actual = cmdline.m_out.str();
            // deviceId must be the one from Funambol
            CPPUNIT_ASSERT(boost::contains(actual, "deviceId = fixed-devid"));
            string filtered = injectValues(filterConfig(actual));
            CPPUNIT_ASSERT_EQUAL_DIFF(filterConfig(internalToIni(ScheduleWorldConfig())),
                                      filtered);
            // there should have been comments
            CPPUNIT_ASSERT(actual.size() > filtered.size());
        }

        {
            TestCmdline cmdline("--print-config", "funambol", NULL);
            cmdline.doit();
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
            CPPUNIT_ASSERT_EQUAL_DIFF(filterConfig(internalToIni(FunambolConfig())),
                                      injectValues(filterConfig(cmdline.m_out.str())));
        }
    }

    void testAddSource() {
        string root;
        ScopedEnvChange templates("SYNCEVOLUTION_TEMPLATE_DIR", "/dev/null");
        ScopedEnvChange xdg("XDG_CONFIG_HOME", m_testDir);
        ScopedEnvChange home("HOME", m_testDir);

        testSetupScheduleWorld();

        root = m_testDir;
        root += "/syncevolution/default";

        {
            TestCmdline cmdline("--configure",
                                "--source-property", "uri = dummy",
                                "scheduleworld",
                                "xyz",
                                NULL);
            cmdline.doit();
            string res = scanFiles(root);
            string expected = ScheduleWorldConfig();
            expected += "\n"
                "peers/scheduleworld/sources/xyz/.internal.ini:# adminData = \n"
                "peers/scheduleworld/sources/xyz/.internal.ini:# synthesisID = 0\n"
                "peers/scheduleworld/sources/xyz/config.ini:# sync = disabled\n"
                "peers/scheduleworld/sources/xyz/config.ini:# type = select backend\n"
                "peers/scheduleworld/sources/xyz/config.ini:uri = dummy\n"
                "sources/xyz/config.ini:# type = select backend\n"
                "sources/xyz/config.ini:# evolutionsource = \n"
                "sources/xyz/config.ini:# evolutionuser = \n"
                "sources/xyz/config.ini:# evolutionpassword = ";
            sortConfig(expected);
            CPPUNIT_ASSERT_EQUAL_DIFF(expected, res);
        }
    }

    void testSync() {
        TestCmdline failure("--sync", NULL);
        CPPUNIT_ASSERT(!failure.m_cmdline->parse());
        CPPUNIT_ASSERT_EQUAL_DIFF("", failure.m_out.str());
        CPPUNIT_ASSERT_EQUAL(string("ERROR: missing parameter for '--sync'\n"), lastLine(failure.m_err.str()));

        TestCmdline failure2("--sync", "foo", NULL);
        CPPUNIT_ASSERT(!failure2.m_cmdline->parse());
        CPPUNIT_ASSERT_EQUAL_DIFF("", failure2.m_out.str());
        CPPUNIT_ASSERT_EQUAL(string("ERROR: '--sync foo': not one of the valid values (two-way, slow, refresh-from-client = refresh-client, refresh-from-server = refresh-server = refresh, one-way-from-client = one-way-client, one-way-from-server = one-way-server = one-way, disabled = none)\n"), lastLine(failure2.m_err.str()));

        TestCmdline help("--sync", " ?", NULL);
        help.doit();
        CPPUNIT_ASSERT_EQUAL_DIFF("--sync\n"
                                  "   requests a certain synchronization mode:\n"
                                  "     two-way             = only send/receive changes since last sync\n"
                                  "     slow                = exchange all items\n"
                                  "     refresh-from-client = discard all remote items and replace with\n"
                                  "                           the items on the client\n"
                                  "     refresh-from-server = discard all local items and replace with\n"
                                  "                           the items on the server\n"
                                  "     one-way-from-client = transmit changes from client\n"
                                  "     one-way-from-server = transmit changes from server\n"
                                  "     none (or disabled)  = synchronization disabled\n",
                                  help.m_out.str());
        CPPUNIT_ASSERT_EQUAL_DIFF("", help.m_err.str());

        TestCmdline filter("--sync", "refresh-from-server", NULL);
        CPPUNIT_ASSERT(filter.m_cmdline->parse());
        CPPUNIT_ASSERT(!filter.m_cmdline->run());
        CPPUNIT_ASSERT_EQUAL_DIFF("", filter.m_out.str());
        CPPUNIT_ASSERT_EQUAL_DIFF("sync = refresh-from-server",
                                  string(filter.m_cmdline->m_sourceProps));
        CPPUNIT_ASSERT_EQUAL_DIFF("",
                                  string(filter.m_cmdline->m_syncProps));

        TestCmdline filter2("--source-property", "sync=refresh", NULL);
        CPPUNIT_ASSERT(filter2.m_cmdline->parse());
        CPPUNIT_ASSERT(!filter2.m_cmdline->run());
        CPPUNIT_ASSERT_EQUAL_DIFF("", filter2.m_out.str());
        CPPUNIT_ASSERT_EQUAL_DIFF("sync = refresh",
                                  string(filter2.m_cmdline->m_sourceProps));
        CPPUNIT_ASSERT_EQUAL_DIFF("",
                                  string(filter2.m_cmdline->m_syncProps));
    }

    void testConfigure() {
        ScopedEnvChange templates("SYNCEVOLUTION_TEMPLATE_DIR", "/dev/null");
        ScopedEnvChange xdg("XDG_CONFIG_HOME", m_testDir);
        ScopedEnvChange home("HOME", m_testDir);

        rm_r(m_testDir);
        testSetupScheduleWorld();
        doConfigure(ScheduleWorldConfig(), "sources/addressbook/config.ini:");

        string syncProperties("syncURL:\n"
                              "\n"
                              "username:\n"
                              "\n"
                              "password:\n"
                              "\n"
                              "logdir:\n"
                              "\n"
                              "loglevel:\n"
                              "\n"
                              "printChanges:\n"
                              "\n"
                              "maxlogdirs:\n"
                              "\n"
                              "preventSlowSync:\n"
                              "\n"
                              "useProxy:\n"
                              "\n"
                              "proxyHost:\n"
                              "\n"
                              "proxyUsername:\n"
                              "\n"
                              "proxyPassword:\n"
                              "\n"
                              "clientAuthType:\n"
                              "\n"
                              "RetryDuration:\n"
                              "\n"
                              "RetryInterval:\n"
                              "\n"
                              "remoteIdentifier:\n"
                              "\n"
                              "PeerIsClient:\n"
                              "\n"
                              "PeerName:\n"
                              "\n"
                              "deviceId:\n"
                              "\n"
                              "remoteDeviceId:\n"
                              "\n"
                              "enableWBXML:\n"
                              "\n"
                              "maxMsgSize:\n"
                              "maxObjSize:\n"
                              "\n"
                              "enableCompression:\n"
                              "\n"
                              "SSLServerCertificates:\n"
                              "\n"
                              "SSLVerifyServer:\n"
                              "\n"
                              "SSLVerifyHost:\n"
                              "\n"
                              "WebURL:\n"
                              "\n"
                              "IconURI:\n"
                              "\n"
                              "ConsumerReady:\n"
                              "\n"
                              "defaultPeer:\n");
        string sourceProperties("sync:\n"
                                "\n"
                                "type:\n"
                                "\n"
                                "evolutionsource:\n"
                                "\n"
                                "uri:\n"
                                "\n"
                                "evolutionuser:\n"
                                "evolutionpassword:\n");

        {
            TestCmdline cmdline("--sync-property", "?",
                                NULL);
            cmdline.doit();
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
            CPPUNIT_ASSERT_EQUAL_DIFF(syncProperties,
                                      filterIndented(cmdline.m_out.str()));
        }

        {
            TestCmdline cmdline("--source-property", "?",
                                NULL);
            cmdline.doit();
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
            CPPUNIT_ASSERT_EQUAL_DIFF(sourceProperties,
                                      filterIndented(cmdline.m_out.str()));
        }

        {
            TestCmdline cmdline("--source-property", "?",
                                "--sync-property", "?",
                                NULL);
            cmdline.doit();
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
            CPPUNIT_ASSERT_EQUAL_DIFF(sourceProperties + syncProperties,
                                      filterIndented(cmdline.m_out.str()));
        }

        {
            TestCmdline cmdline("--sync-property", "?",
                                "--source-property", "?",
                                NULL);
            cmdline.doit();
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
            CPPUNIT_ASSERT_EQUAL_DIFF(syncProperties + sourceProperties,
                                      filterIndented(cmdline.m_out.str()));
        }
    }

    void testOldConfigure() {
        ScopedEnvChange templates("SYNCEVOLUTION_TEMPLATE_DIR", "/dev/null");
        ScopedEnvChange xdg("XDG_CONFIG_HOME", m_testDir);
        ScopedEnvChange home("HOME", m_testDir);

        string oldConfig = OldScheduleWorldConfig();
        InitList<string> props = InitList<string>("serverNonce") +
            "clientNonce" +
            "devInfoHash" +
            "HashCode" +
            "ConfigDate" +
            "deviceData" +
            "adminData" +
            "synthesisID" +
            "lastNonce" +
            "last";
        BOOST_FOREACH(string &prop, props) {
            boost::replace_all(oldConfig,
                               prop + " = ",
                               prop + " = internal value");
        }

        rm_r(m_testDir);
        createFiles(m_testDir + "/.sync4j/evolution/scheduleworld", oldConfig);
        doConfigure(oldConfig, "spds/sources/addressbook/config.txt:");
    }

    void doConfigure(const string &SWConfig, const string &addressbookPrefix) {
        string expected;

        {
            TestCmdline cmdline("--configure",
                                "--source-property", "sync = disabled",
                                "scheduleworld",
                                NULL);
            cmdline.doit();
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_out.str());
            expected = filterConfig(internalToIni(SWConfig));
            boost::replace_all(expected,
                               "sync = two-way",
                               "sync = disabled");
            CPPUNIT_ASSERT_EQUAL_DIFF(expected,
                                      filterConfig(printConfig("scheduleworld")));
        }

        {
            TestCmdline cmdline("--configure",
                                "--source-property", "sync = one-way-from-server",
                                "scheduleworld",
                                "addressbook",
                                NULL);
            cmdline.doit();
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_out.str());
            expected = SWConfig;
            boost::replace_all(expected,
                               "sync = two-way",
                               "sync = disabled");
            boost::replace_first(expected,
                                 addressbookPrefix + "sync = disabled",
                                 addressbookPrefix + "sync = one-way-from-server");
            expected = filterConfig(internalToIni(expected));
            CPPUNIT_ASSERT_EQUAL_DIFF(expected,
                                      filterConfig(printConfig("scheduleworld")));
        }

        {
            TestCmdline cmdline("--configure",
                                "--sync", "two-way",
                                "-z", "evolutionsource=source",
                                "--sync-property", "maxlogdirs=20",
                                "-y", "LOGDIR=logdir",
                                "scheduleworld",
                                NULL);
            cmdline.doit();
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_out.str());
            boost::replace_all(expected,
                               "sync = one-way-from-server",
                               "sync = two-way");
            boost::replace_all(expected,
                               "sync = disabled",
                               "sync = two-way");
            boost::replace_all(expected,
                               "# evolutionsource = ",
                               "evolutionsource = source");
            boost::replace_all(expected,
                               "# maxlogdirs = 10",
                               "maxlogdirs = 20");
            boost::replace_all(expected,
                               "# logdir = ",
                               "logdir = logdir");
            CPPUNIT_ASSERT_EQUAL_DIFF(expected,
                                      filterConfig(printConfig("scheduleworld")));
        }
    }

    void testListSources() {
        // pick the varargs constructor; NULL alone is ambiguous
        TestCmdline cmdline(NULL, NULL);
        cmdline.doit();
        CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
        // exact output varies, do not test
    }

    void testMigrate() {
        ScopedEnvChange templates("SYNCEVOLUTION_TEMPLATE_DIR", "/dev/null");
        ScopedEnvChange xdg("XDG_CONFIG_HOME", m_testDir);
        ScopedEnvChange home("HOME", m_testDir);

        rm_r(m_testDir);
        string oldRoot = m_testDir + "/.sync4j/evolution/scheduleworld";
        string newRoot = m_testDir + "/syncevolution/default";

        string oldConfig = OldScheduleWorldConfig();

        {
            // migrate old config
            createFiles(oldRoot, oldConfig);
            string createdConfig = scanFiles(oldRoot);
            TestCmdline cmdline("--migrate",
                                "scheduleworld",
                                NULL);
            cmdline.doit();
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_out.str());

            string migratedConfig = scanFiles(newRoot);
            string expected = ScheduleWorldConfig();
            sortConfig(expected);
            CPPUNIT_ASSERT_EQUAL_DIFF(expected, migratedConfig);
            string renamedConfig = scanFiles(oldRoot + ".old");
            CPPUNIT_ASSERT_EQUAL_DIFF(createdConfig, renamedConfig);
        }

        {
            // rewrite existing config with obsolete properties
            // => these properties should get removed
            //
            // There is one limitation: shared nodes are not rewritten.
            // This is acceptable.
            createFiles(newRoot + "/peers/scheduleworld",
                        "config.ini:# obsolete comment\n"
                        "config.ini:obsoleteprop = foo\n",
                        true);
            string createdConfig = scanFiles(newRoot, "scheduleworld");

            TestCmdline cmdline("--migrate",
                                "scheduleworld",
                                NULL);
            cmdline.doit();
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_out.str());

            string migratedConfig = scanFiles(newRoot, "scheduleworld");
            string expected = ScheduleWorldConfig();
            sortConfig(expected);
            CPPUNIT_ASSERT_EQUAL_DIFF(expected, migratedConfig);
            string renamedConfig = scanFiles(newRoot, "scheduleworld.old");
            boost::replace_all(createdConfig, "/scheduleworld/", "/scheduleworld.old/");
            CPPUNIT_ASSERT_EQUAL_DIFF(createdConfig, renamedConfig);
        }

        {
            // migrate old config with changes and .synthesis directory, a second time
            createFiles(oldRoot, oldConfig);
            createFiles(oldRoot,
                        ".synthesis/dummy-file.bfi:dummy = foobar\n"
                        "spds/sources/addressbook/changes/config.txt:foo = bar\n"
                        "spds/sources/addressbook/changes/config.txt:foo2 = bar2\n",
                        true);
            string createdConfig = scanFiles(oldRoot);
            rm_r(newRoot);
            TestCmdline cmdline("--migrate",
                                "scheduleworld",
                                NULL);
            cmdline.doit();
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
            CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_out.str());

            string migratedConfig = scanFiles(newRoot);
            string expected = m_scheduleWorldConfig;
            sortConfig(expected);
            boost::replace_first(expected,
                                 "peers/scheduleworld/sources/addressbook/config.ini",
                                 "peers/scheduleworld/sources/addressbook/.other.ini:foo = bar\n"
                                 "peers/scheduleworld/sources/addressbook/.other.ini:foo2 = bar2\n"
                                 "peers/scheduleworld/sources/addressbook/config.ini");
            boost::replace_first(expected,
                                 "peers/scheduleworld/config.ini",
                                 "peers/scheduleworld/.synthesis/dummy-file.bfi:dummy = foobar\n"
                                 "peers/scheduleworld/config.ini");
            CPPUNIT_ASSERT_EQUAL_DIFF(expected, migratedConfig);
            string renamedConfig = scanFiles(oldRoot + ".old.1");
            CPPUNIT_ASSERT_EQUAL_DIFF(createdConfig, renamedConfig);
        }

        {
            string otherRoot = m_testDir + "/syncevolution/other";
            rm_r(otherRoot);

            // migrate old config into non-default context
            createFiles(oldRoot, oldConfig);
            string createdConfig = scanFiles(oldRoot);
            {
                TestCmdline cmdline("--migrate",
                                    "scheduleworld@other",
                                    NULL);
                cmdline.doit();
                CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
                CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_out.str());
            }

            string migratedConfig = scanFiles(otherRoot);
            string expected = ScheduleWorldConfig();
            sortConfig(expected);
            CPPUNIT_ASSERT_EQUAL_DIFF(expected, migratedConfig);
            string renamedConfig = scanFiles(oldRoot + ".old");
            CPPUNIT_ASSERT_EQUAL_DIFF(createdConfig, renamedConfig);

            // migrate the migrated config again inside the "other" context
            {
                TestCmdline cmdline("--migrate",
                                    "scheduleworld@other",
                                    NULL);
                cmdline.doit();
                CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
                CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_out.str());
            }
            migratedConfig = scanFiles(otherRoot, "scheduleworld");
            expected = ScheduleWorldConfig();
            sortConfig(expected);
            CPPUNIT_ASSERT_EQUAL_DIFF(expected, migratedConfig);
            renamedConfig = scanFiles(otherRoot, "scheduleworld.old");
            boost::replace_all(expected, "/scheduleworld/", "/scheduleworld.old/");
            CPPUNIT_ASSERT_EQUAL_DIFF(expected, renamedConfig);
        }
    }

    const string m_testDir;
    string m_scheduleWorldConfig;
        

private:

    /**
     * vararg constructor with NULL termination,
     * out and error stream into stringstream members
     */
    class TestCmdline {
        void init() {
            m_argv.reset(new const char *[m_argvstr.size() + 1]);
            m_argv[0] = "client-test";
            for (size_t index = 0;
                 index < m_argvstr.size();
                 ++index) {
                m_argv[index + 1] = m_argvstr[index].c_str();
            }

            m_cmdline.set(new Cmdline(m_argvstr.size() + 1, m_argv.get(), m_out, m_err), "cmdline");
            
        }

    public:
        TestCmdline(const char *arg, ...) {
            va_list argList;
            va_start (argList, arg);
            for (const char *curr = arg;
                 curr;
                 curr = va_arg(argList, const char *)) {
                m_argvstr.push_back(curr);
            }
            va_end(argList);
            init();
        }

        TestCmdline(const char * const argv[]) {
            for (int i = 0; argv[i]; i++) {
                m_argvstr.push_back(argv[i]);
            }
            init();
        }

        void doit() {
            bool success;
            success = m_cmdline->parse() &&
                m_cmdline->run();
            if (m_err.str().size()) {
                cout << endl << m_err.str();
            }
            CPPUNIT_ASSERT(success);
        }

        ostringstream m_out, m_err;
        cxxptr<Cmdline> m_cmdline;

    private:
        vector<string> m_argvstr;
        boost::scoped_array<const char *> m_argv;
    };

    string ScheduleWorldConfig() {
        string config = m_scheduleWorldConfig;

#if 0
        // Currently we don't have an icon for ScheduleWorld. If we
        // had (MB #2062) one, then this code would ensure that the
        // reference config also has the right path for it.
        const char *templateDir = getenv("SYNCEVOLUTION_TEMPLATE_DIR");
        if (!templateDir) {
            templateDir = TEMPLATE_DIR;
        }


        if (isDir(string(templateDir) + "/ScheduleWorld")) {
            boost::replace_all(config,
                               "# IconURI = ",
                               string("IconURI = file://") + templateDir + "/ScheduleWorld/icon.png");
        }
#endif
        return config;
    }

    string OldScheduleWorldConfig() {
        // old style paths
        string oldConfig =
            "spds/syncml/config.txt:syncURL = http://sync.scheduleworld.com/funambol/ds\n"
            "spds/syncml/config.txt:username = your SyncML server account name\n"
            "spds/syncml/config.txt:password = your SyncML server password\n"
            "spds/syncml/config.txt:# logdir = \n"
            "spds/syncml/config.txt:# loglevel = 0\n"
            "spds/syncml/config.txt:# printChanges = 1\n"
            "spds/syncml/config.txt:# maxlogdirs = 10\n"
            "spds/syncml/config.txt:# preventSlowSync = 0\n"
            "spds/syncml/config.txt:# useProxy = 0\n"
            "spds/syncml/config.txt:# proxyHost = \n"
            "spds/syncml/config.txt:# proxyUsername = \n"
            "spds/syncml/config.txt:# proxyPassword = \n"
            "spds/syncml/config.txt:# clientAuthType = md5\n"
            "spds/syncml/config.txt:# RetryDuration = 300\n"
            "spds/syncml/config.txt:# RetryInterval = 60\n"
            "spds/syncml/config.txt:# remoteIdentifier = \n"
            "spds/syncml/config.txt:# PeerIsClient = 0\n"
            "spds/syncml/config.txt:# PeerName = \n"
            "spds/syncml/config.txt:deviceId = fixed-devid\n" /* this is not the default! */
            "spds/syncml/config.txt:# remoteDeviceId = \n"
            "spds/syncml/config.txt:# enableWBXML = 1\n"
            "spds/syncml/config.txt:# maxMsgSize = 20000\n"
            "spds/syncml/config.txt:# maxObjSize = 4000000\n"
            "spds/syncml/config.txt:# enableCompression = 0\n"
#ifdef ENABLE_LIBSOUP
            // path to SSL certificates is only set for libsoup
            "spds/syncml/config.txt:# SSLServerCertificates = /etc/ssl/certs/ca-certificates.crt:/etc/pki/tls/certs/ca-bundle.crt:/usr/share/ssl/certs/ca-bundle.crt\n"

#else
            "spds/syncml/config.txt:# SSLServerCertificates = \n"
#endif
            "spds/syncml/config.txt:# SSLVerifyServer = 1\n"
            "spds/syncml/config.txt:# SSLVerifyHost = 1\n"
            "spds/syncml/config.txt:WebURL = http://www.scheduleworld.com\n"
            "spds/syncml/config.txt:# IconURI = \n"
            "spds/syncml/config.txt:ConsumerReady = 1\n"
            "spds/sources/addressbook/config.txt:sync = two-way\n"
            "spds/sources/addressbook/config.txt:type = addressbook:text/vcard\n"
            "spds/sources/addressbook/config.txt:# evolutionsource = \n"
            "spds/sources/addressbook/config.txt:uri = card3\n"
            "spds/sources/addressbook/config.txt:# evolutionuser = \n"
            "spds/sources/addressbook/config.txt:# evolutionpassword = \n"
            "spds/sources/calendar/config.txt:sync = two-way\n"
            "spds/sources/calendar/config.txt:type = calendar\n"
            "spds/sources/calendar/config.txt:# evolutionsource = \n"
            "spds/sources/calendar/config.txt:uri = cal2\n"
            "spds/sources/calendar/config.txt:# evolutionuser = \n"
            "spds/sources/calendar/config.txt:# evolutionpassword = \n"
            "spds/sources/memo/config.txt:sync = two-way\n"
            "spds/sources/memo/config.txt:type = memo\n"
            "spds/sources/memo/config.txt:# evolutionsource = \n"
            "spds/sources/memo/config.txt:uri = note\n"
            "spds/sources/memo/config.txt:# evolutionuser = \n"
            "spds/sources/memo/config.txt:# evolutionpassword = \n"
            "spds/sources/todo/config.txt:sync = two-way\n"
            "spds/sources/todo/config.txt:type = todo\n"
            "spds/sources/todo/config.txt:# evolutionsource = \n"
            "spds/sources/todo/config.txt:uri = task2\n"
            "spds/sources/todo/config.txt:# evolutionuser = \n"
            "spds/sources/todo/config.txt:# evolutionpassword = \n";
        return oldConfig;
    }

    string FunambolConfig() {
        string config = m_scheduleWorldConfig;
        boost::replace_all(config, "/scheduleworld/", "/funambol/");

        boost::replace_first(config,
                             "syncURL = http://sync.scheduleworld.com/funambol/ds",
                             "syncURL = http://my.funambol.com/sync");

        boost::replace_first(config,
                             "WebURL = http://www.scheduleworld.com",
                             "WebURL = http://my.funambol.com");

        boost::replace_first(config,
                             "# enableWBXML = 1",
                             "enableWBXML = 0");

        boost::replace_first(config,
                             "addressbook/config.ini:uri = card3",
                             "addressbook/config.ini:uri = card");
        boost::replace_all(config,
                           "addressbook/config.ini:type = addressbook:text/vcard",
                           "addressbook/config.ini:type = addressbook");

        boost::replace_first(config,
                             "calendar/config.ini:uri = cal2",
                             "calendar/config.ini:uri = event");
        boost::replace_all(config,
                           "calendar/config.ini:type = calendar",
                           "calendar/config.ini:type = calendar:text/calendar!");

        boost::replace_first(config,
                             "todo/config.ini:uri = task2",
                             "todo/config.ini:uri = task");
        boost::replace_all(config,
                           "todo/config.ini:type = todo",
                           "todo/config.ini:type = todo:text/calendar!");

        return config;
    }

    string SynthesisConfig() {
        string config = m_scheduleWorldConfig;
        boost::replace_all(config, "/scheduleworld/", "/synthesis/");

        boost::replace_first(config,
                             "syncURL = http://sync.scheduleworld.com/funambol/ds",
                             "syncURL = http://www.synthesis.ch/sync");

        boost::replace_first(config,
                             "WebURL = http://www.scheduleworld.com",
                             "WebURL = http://www.synthesis.ch");        

        boost::replace_first(config,
                             "ConsumerReady = 1",
                             "# ConsumerReady = 0");

        boost::replace_first(config,
                             "addressbook/config.ini:uri = card3",
                             "addressbook/config.ini:uri = contacts");
        boost::replace_all(config,
                           "addressbook/config.ini:type = addressbook:text/vcard",
                           "addressbook/config.ini:type = addressbook");

        boost::replace_first(config,
                             "calendar/config.ini:uri = cal2",
                             "calendar/config.ini:uri = events");
        boost::replace_first(config,
                             "calendar/config.ini:sync = two-way",
                             "calendar/config.ini:sync = disabled");

        boost::replace_first(config,
                             "memo/config.ini:uri = note",
                             "memo/config.ini:uri = notes");

        boost::replace_first(config,
                             "todo/config.ini:uri = task2",
                             "todo/config.ini:uri = tasks");
        boost::replace_first(config,
                             "todo/config.ini:sync = two-way",
                             "todo/config.ini:sync = disabled");

        return config;
    }          

    /** create directory hierarchy, overwriting previous content */
    void createFiles(const string &root, const string &content, bool append = false) {
        if (!append) {
            rm_r(root);
        }

        size_t start = 0;
        ofstream out;
        string outname;

        out.exceptions(ios_base::badbit|ios_base::failbit);
        while (start < content.size()) {
            size_t delim = content.find(':', start);
            size_t end = content.find('\n', start);
            if (delim == content.npos ||
                end == content.npos) {
                // invalid content ?!
                break;
            }
            string newname = content.substr(start, delim - start);
            string line = content.substr(delim + 1, end - delim - 1);
            if (newname != outname) {
                if (out.is_open()) {
                    out.close();
                }
                string fullpath = root + "/" + newname;
                size_t fileoff = fullpath.rfind('/');
                mkdir_p(fullpath.substr(0, fileoff));
                out.open(fullpath.c_str(),
                         append ? ios_base::out : (ios_base::out|ios_base::trunc));
                outname = newname;
            }
            out << line << endl;
            start = end + 1;
        }
    }

    /** turn directory hierarchy into string
     *
     * @param root       root path in file system
     * @param peer       if non-empty, then ignore all <root>/peers/<foo> directories
     *                   where <foo> != peer
     * @param onlyProps  ignore lines which are comments
     */
    string scanFiles(const string &root, const string &peer = "", bool onlyProps = true) {
        ostringstream out;

        scanFiles(root, "", peer, out, onlyProps);
        return out.str();
    }

    void scanFiles(const string &root, const string &dir, const string &peer, ostringstream &out, bool onlyProps) {
        string newroot = root;
        newroot += "/";
        newroot += dir;
        ReadDir readDir(newroot);
        sort(readDir.begin(), readDir.end());

        BOOST_FOREACH(const string &entry, readDir) {
            if (isDir(newroot + "/" + entry)) {
                if (boost::ends_with(newroot, "/peers") &&
                    !peer.empty() &&
                    entry != peer) {
                    // skip different peer directory
                    continue;
                } else {
                    scanFiles(root, dir + (dir.empty() ? "" : "/") + entry, peer, out, onlyProps);
                }
            } else {
                ifstream in;
                in.exceptions(ios_base::badbit /* failbit must not trigger exception because is set when reaching eof ?! */);
                in.open((newroot + "/" + entry).c_str());
                string line;
                while (!in.eof()) {
                    getline(in, line);
                    if ((line.size() || !in.eof()) && 
                        (!onlyProps ||
                         (boost::starts_with(line, "# ") ?
                          isPropAssignment(line.substr(2)) :
                          !line.empty()))) {
                        if (dir.size()) {
                            out << dir << "/";
                        }
                        out << entry << ":";
                        out << line << '\n';
                    }
                }
            }
        }
    }

    string printConfig(const string &server) {
        ScopedEnvChange templates("SYNCEVOLUTION_TEMPLATE_DIR", "/dev/null");
        ScopedEnvChange xdg("XDG_CONFIG_HOME", m_testDir);
        ScopedEnvChange home("HOME", m_testDir);

        TestCmdline cmdline("--print-config", server.c_str(), NULL);
        cmdline.doit();
        CPPUNIT_ASSERT_EQUAL_DIFF("", cmdline.m_err.str());
        return cmdline.m_out.str();
    }
};

SYNCEVOLUTION_TEST_SUITE_REGISTRATION(CmdlineTest);

#endif // ENABLE_UNIT_TESTS

SE_END_CXX
