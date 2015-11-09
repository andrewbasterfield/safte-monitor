Umask 026

Tuning {
	NumConnections 15
	BufSize 8192
}

User daemon
StayRoot On

PIDFile _localstatedir_/run/safte-monitor/pid
ErrorLog _localstatedir_/log/safte-monitor/error_log

Control {
	Symlinks On
	Types {
		text/plain { * }
		text/css { css }
		text/html { html htm }
		image/gif { gif }
		image/jpeg { jpg }
	}
	Specials {
		Redirect { url }
		safte-monitor { safte }
	}
	IndexNames { index.url }
}

DefaultName localhost

Server {
	Port 8123

	Virtual {
		Control {
			Alias /
			Location _pkgdatadir_/www
			Realm "safte-monitor"
			UserFile _sysconfdir_/safte-monitor.passwd
		}
	}

}

