// ui/src/App.jsx
import React from 'react';
import { useDaemon } from './hooks/useDaemon';

export default function App() {
  const { isConnected, metrics, setProfile } = useDaemon();
  const activeProfile = metrics?.profile || 'Unknown';
  
  // [+] Extract power status from the C++ metrics payload. Default to false if missing.
  const isPluggedIn = metrics?.onACPower ?? false; 

  return (
    <div style={{ padding: '2rem', maxWidth: '600px', margin: '0 auto', fontFamily: 'sans-serif' }}>
      <header style={{ borderBottom: '1px solid #333', paddingBottom: '1rem', marginBottom: '2rem' }}>
        <h1 style={{ margin: 0, color: '#00d2ff', letterSpacing: '1px' }}>MacMS Alpha</h1>
        <p style={{ margin: '0.5rem 0 0 0', color: '#888' }}>
          IPC Link: <span style={{ color: isConnected ? '#00ff00' : '#ff4444', fontWeight: 'bold' }}>
            {isConnected ? 'ONLINE' : 'OFFLINE'}
          </span>
          {/* [+] Added a visual Power status indicator to the UI header */}
          {' '} | Power: <span style={{ color: isPluggedIn ? '#00ff00' : '#ffaa00', fontWeight: 'bold' }}>
            {isPluggedIn ? 'AC ADAPTER' : 'BATTERY'}
          </span>
        </p>
      </header>

      <section style={{ backgroundColor: '#1e1e1e', padding: '1.5rem', borderRadius: '8px', border: '1px solid #333' }}>
        <h2 style={{ marginTop: 0 }}>System Telemetry</h2>

        {metrics ? (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '1rem', marginBottom: '2rem' }}>
            <div style={metricBoxStyle}>
              <div>CPU Temp</div>
              <div style={{ fontSize: '1.5rem', color: '#fff' }}>{metrics.cpuTemp.toFixed(1)}°C</div>
            </div>
            <div style={metricBoxStyle}>
              <div>GPU Die</div>
              <div style={{ fontSize: '1.5rem', color: '#fff' }}>{metrics.gpuTemp.toFixed(1)}°C</div>
            </div>
            <div style={metricBoxStyle}>
              <div>Left Fan</div>
              <div style={{ fontSize: '1.5rem', color: '#fff' }}>{metrics.fanLeftRpm} RPM</div>
            </div>
            <div style={metricBoxStyle}>
              <div>Right Fan</div>
              <div style={{ fontSize: '1.5rem', color: '#fff' }}>{metrics.fanRightRpm} RPM</div>
            </div>
          </div>
        ) : (
          <p style={{ color: '#aaa', fontSize: '0.9rem', marginBottom: '2rem' }}>
            Waiting for hardware sensor stream...
          </p>
        )}
        
        <div>
          <h3>Current Profile: <span style={{ color: '#00d2ff' }}>{activeProfile}</span></h3>
          
          <div style={{ display: 'flex', gap: '10px', marginTop: '1rem' }}>
            <button 
              onClick={() => setProfile('Silent')}
              style={buttonStyle(activeProfile === 'Silent', false, !isConnected)}
              disabled={!isConnected}
            >
              Silent
            </button>
            <button 
              onClick={() => setProfile('Normal')}
              style={buttonStyle(activeProfile === 'Normal', false, !isConnected)}
              disabled={!isConnected}
            >
              Normal
            </button>
            <button 
              onClick={() => setProfile('Performance')}
              style={buttonStyle(activeProfile === 'Performance', false, !isConnected)}
              disabled={!isConnected}
            >
              Performance
            </button>
            <button 
              onClick={() => setProfile('Maximum')}
              // [+] Pass the power state to the style function to visually gray it out
              style={buttonStyle(activeProfile === 'Maximum', true, !isConnected || !isPluggedIn)}
              // [+] Hard-disable the click action if disconnected OR on battery
              disabled={!isConnected || !isPluggedIn}
              // [+] Add a tooltip so the user knows why it is disabled
              title={!isPluggedIn ? "Must be plugged into AC Power to use Maximum Mode" : ""}
            >
              Maximum
            </button>
          </div>

          {metrics?.sessionActive && (
            <div style={{ marginTop: '1.5rem', padding: '1rem', backgroundColor: '#332b00', borderLeft: '4px solid #ffcc00', color: '#ffd700', borderRadius: '4px' }}>
              <strong>Safety Timer Active:</strong> High-load session running for {metrics.sessionDurationMin} minutes.
            </div>
          )}
        </div>
      </section>
    </div>
  );
}

const metricBoxStyle = {
  backgroundColor: '#2a2a2a',
  padding: '1rem',
  borderRadius: '6px',
  color: '#aaa',
  fontSize: '0.9rem'
};

// [+] Added `isDisabled` parameter to dynamically change the styling
function buttonStyle(isActive, isDanger = false, isDisabled = false) { 
  return {
    padding: '8px 16px',
    backgroundColor: isActive ? (isDanger ? '#ff4444' : '#00d2ff') : '#2a2a2a',
    color: isActive ? '#000' : '#fff',
    border: 'none',
    borderRadius: '4px',
    // [+] Change cursor to "not-allowed" (the red circle slash) if disabled
    cursor: isDisabled ? 'not-allowed' : (isActive ? 'default' : 'pointer'),
    fontWeight: isActive ? 'bold' : 'normal',
    transition: 'background-color 0.2s',
    // [+] Drop the opacity heavily if disabled so it visually looks turned off
    opacity: isDisabled ? 0.2 : (isActive ? 1 : 0.7) 
  };
}