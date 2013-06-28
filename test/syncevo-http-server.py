#! /usr/bin/python

'''Usage: syncevo-http-server.py <URL>
Runs a SyncML HTTP server under the given base URL.'''

# use the same glib main loop in D-Bus and twisted
from dbus.mainloop.glib import DBusGMainLoop
from twisted.internet import glib2reactor # for non-GUI apps
DBusGMainLoop(set_as_default=True)
glib2reactor.install()

import dbus
import gobject
import sys
import urlparse
import optparse

import twisted.web
from twisted.web import server, resource, http
from twisted.internet import reactor

bus = dbus.SessionBus()
loop = gobject.MainLoop()

# cached information about previous POST and reply,
# in case that we need to resend
class OldRequest:
    sessionid = None
    data = None
    reply = None
    type = None

def session_changed(object, ready):
    print "SessionChanged:", object, ready

bus.add_signal_receiver(session_changed,
                        'SessionChanged',
                        'org.syncevolution.Server',
                        'org.syncevolution',
                        None,
                        byte_arrays=True)

class SyncMLSession:
    sessions = []

    def __init__(self):
        self.sessionid = None
        self.request = None
        self.conpath = None
        self.connection = None

    def destruct(self, code, message=""):
        '''Tell both HTTP client and D-Bus server that we are shutting down,
        then remove the session'''
        if self.request:
            self.request.setResponseCode(code, message)
            self.request.finish()
            self.request = None
        if self.connection:
            self.connection.Close(False, message)
            self.connection = None
        if self in SyncMLSession.sessions:
            SyncMLSession.sessions.remove(self)

    def abort(self):
        '''D-Bus server requests to close connection, so cancel everything'''
        print "connection", self.conpath, "went down"
        self.destruct(http.INTERNAL_SERVER_ERROR, "lost connection to SyncEvolution")

    def reply(self, data, type, meta, final, session):
        '''sent reply to HTTP client and/or close down normally'''
        print "reply session", session, "final", final, "data len", len(data), meta
        # When the D-Bus server sends an empty array, Python binding
        # puts the four chars in 'None' into the data array?!
        if data and len(data) > 0 and data != 'None':
            request = self.request
            self.request = None
            OldRequest.reply = data
            OldRequest.type = type
            if request:
                request.setHeader('Content-Type', type)
                request.setResponseCode(http.OK)
                request.write(data)
                request.finish()
                self.sessionid = session
            else:
                self.connection.Close(False, "could not deliver reply")
                self.connection = None
        if final:
            print "closing connection for connection", self.conpath, "session", session
            if self.connection:
                self.connection.Close(True, "")
                self.connection = None
            self.destruct(http.GONE, "D-Bus server done")

    def done(self, error):
        '''lost connection to HTTP client, either normally or in error'''
        if error and self.connection:
            self.connection.Close(False, error)
            self.connection = None

    def start(self, request, config, url):
        '''start a new session based on the incoming message'''
        print "requesting new session"
        self.object = dbus.Interface(bus.get_object('org.syncevolution',
                                                    '/org/syncevolution/Server'),
                                     'org.syncevolution.Server')
        deferred = request.notifyFinish()
        deferred.addCallback(self.done)
        self.conpath = self.object.Connect({'description': 'syncevo-server-http.py',
                                            'transport': 'HTTP',
                                            'config': config,
                                            'URL': url},
                                           True,
                                           '')
        self.connection = dbus.Interface(bus.get_object('org.syncevolution',
                                                        self.conpath),
                                         'org.syncevolution.Connection')

        bus.add_signal_receiver(self.abort,
                                'Abort',
                                'org.syncevolution.Connection',
                                'org.syncevolution',
                                self.conpath,
                                utf8_strings=True,
                                byte_arrays=True)
        bus.add_signal_receiver(self.reply,
                                'Reply',
                                'org.syncevolution.Connection',
                                'org.syncevolution',
                                self.conpath,
                                utf8_strings=True,
                                byte_arrays=True)

        # feed new data into SyncEvolution and wait for reply
        request.content.seek(0, 0)
        self.connection.Process(request.content.read(),
                                request.getHeader('content-type'))
        self.request = request
        SyncMLSession.sessions.append(self)

    def process(self, request, data):
        '''process next message by client in running session'''
        if self.request:
            # message resend?! Ignore old request.
            print "message resend?!"
            self.request.finish()
            self.request = None
        deferred = request.notifyFinish()
        deferred.addCallback(self.done)
        self.connection.Process(data,
                                request.getHeader('content-type'))
        self.request = request

class SyncMLPost(resource.Resource):
    isLeaf = True

    def __init__(self, url):
        self.url = url

    def render_GET(self, request):
        return "<html>SyncEvolution SyncML Server</html>"

    def render_POST(self, request):
        config = request.postpath
        if config:
            config = config[0]
        else:
            config = ""
        type = request.getHeader('content-type')
        len = request.getHeader('content-length')
        sessionid = request.args.get('sessionid')
        if sessionid:
            sessionid = sessionid[0]
        print "POST from", request.getClientIP(), "config", config, "type", type, "session", sessionid, "args", request.args, "length", len
        if not sessionid:
            session = SyncMLSession()
            session.start(request, config,
                          urlparse.urljoin(self.url.geturl(), request.path))
            return server.NOT_DONE_YET
        else:
            data = request.content.read()
            # Detect resent message. We support that for
            # independently from the session, because it
            # might already be gone (server sends last reply
            # in session, closes session, client doesn't
            # get reply, reposts).
            if sessionid == OldRequest.sessionid and \
                    OldRequest.data == data and \
                    OldRequest.reply:
                print "resend reply session", sessionid
                request.setHeader('Content-Type', OldRequest.type)
                request.setResponseCode(http.OK)
                request.write(OldRequest.reply)
                request.finish()
                return server.NOT_DONE_YET
            else:
                # prepare resending, will be completed in
                # SyncSession.reply()
                OldRequest.sessionid = sessionid
                OldRequest.data = data
                OldRequest.reply = None
            for session in SyncMLSession.sessions:
                if session.sessionid == sessionid:
                    session.process(request, data)
                    return server.NOT_DONE_YET
            print "unknown session", sessionid, "=>", http.NOT_FOUND
            raise twisted.web.Error(http.NOT_FOUND)


usage =  """usage: %prog http://localhost:<port>/<path>

Runs a HTTP server which listens on all network interfaces on
the given port and answers requests for the given path.
Configurations for clients must be created manually, see
http://syncevolution.org/development/http-server-howto"""

def main():
    parser = optparse.OptionParser(usage=usage)
    (options, args) = parser.parse_args()
    if len(args) != 1:
        print "need exactly on URL as command line parameter"
        exit(1)
    url = urlparse.urlparse(args[0])
    root = resource.Resource()
    root.putChild(url.path[1:], SyncMLPost(url))
    site = server.Site(root)
    reactor.listenTCP(url.port, site)
    reactor.run()

if __name__ == '__main__':
    main()
