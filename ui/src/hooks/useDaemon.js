// ui/src/hooks/useDaemon.js
import { useEffect, useRef, useState, useCallback } from 'react';

const WS_URL = 'ws://127.0.0.1:8080';

export function useDaemon() {
  const wsRef = useRef(null);
  const [isConnected, setIsConnected] = useState(false);
  const [metrics, setMetrics] = useState(null);

  useEffect(() => {
    // Each effect run gets its own flags, so React.StrictMode's
    // mount -> unmount -> mount cycle can't mix up two sockets.
    let cancelled = false;
    let retryTimer = null;

    const connect = () => {
      const ws = new WebSocket(WS_URL);
      wsRef.current = ws;

      ws.onopen = () => {
        if (wsRef.current === ws) setIsConnected(true);
      };

      ws.onmessage = (event) => {
        try {
          const msg = JSON.parse(event.data);
          if (msg.type === 'metrics') setMetrics(msg.data);
        } catch (err) {
          console.error('[useDaemon] Bad JSON from daemon:', err);
        }
      };

      ws.onclose = () => {
        // Only react if this is still the *current* socket.
        if (wsRef.current === ws) {
          wsRef.current = null;
          setIsConnected(false);
        }
        if (!cancelled) retryTimer = setTimeout(connect, 2000);
      };

      ws.onerror = () => ws.close();
    };

    connect();

    return () => {
      cancelled = true;
      clearTimeout(retryTimer);
      const ws = wsRef.current;
      wsRef.current = null;
      if (ws) ws.close();
      setIsConnected(false);
    };
  }, []);

  const setProfile = useCallback((profile) => {
    const ws = wsRef.current;
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      console.warn('[useDaemon] Socket not open, cannot send profile:', profile);
      return;
    }
    // Must match handleMessage() in websocket_server.cpp exactly:
    // json["type"] == "setProfile" and json["profile"] is a string.
    const payload = JSON.stringify({ type: 'setProfile', profile });
    console.log('[useDaemon] sending', payload);
    ws.send(payload);
  }, []);

  return { isConnected, metrics, setProfile };
}