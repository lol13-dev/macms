// folder location: ui/src/hooks/useDaemon.js
import { useState, useEffect, useRef, useCallback } from 'react';

export function useDaemon(url = 'ws://127.0.0.1:8080') {
    const [isConnected, setIsConnected] = useState(false);
    const [metrics, setMetrics] = useState(null);
    const wsRef = useRef(null);

    useEffect(() => {
        let reconnectTimer;

        const connect = () => {
            const ws = new WebSocket(url);
            wsRef.current = ws;

            ws.onopen = () => {
                setIsConnected(true);
                console.log('[MacMS_STATUS] CONNECTED to C++ Daemon');
            };

            ws.onmessage = (event) => {
                try {
                    const payload = JSON.parse(event.data);
                    if (payload.type == 'metrics') {
                        setMetrics(payload.data);
                    }
                } catch (err) {
                    console.error('[MacMS_STATUS] Failed to parse payload:', err);
                }
            };

            ws.onclose = () => {
                setIsConnected(false);
                wsRef.current = null;

                // CHECK every 2 sec if the daemon is BACK ONLINE.
                reconnectTimer = setTimeout(connect, 2000);
            };

            ws.onerror = (err) => {
                // SUPPRESS noisy ERROR logs when the daemons 
                ws.close();
            };
        };

        connect();

        return () => {
            clearTimeout(reconnectTimer);
            if (wsRef.current) {
                wsRef.current.close();
            }
        };
    }, [url]);

    const setProfile = useCallback((profilename) => {
        if (wsRef.current && wsRef.current.readyState === webSocket.OPEN) {
            wsRef.current.send(JSON.stringify ({
                type: 'setProfile',
                profile: profileName
            }));
        } else {
            console.warn('[MacMS_NOTE] Cannot change profile. Daemon is disconnected.')
        }
    }, []);

    return { isConnected, metrics, setProfile };
}