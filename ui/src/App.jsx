// ui/src/App.jsx
import React, { useState } from 'react';

export default function App() {
  // Local state just for testing the UI visuals before we hook up C++
  const [activeProfile, setActiveProfile] = useState('Normal');

  return (
    <div style={{ padding: '2rem', maxWidth: '600px', margin: '0 auto', fontFamily: 'sans-serif' }}>
      <header style={{ borderBottom: '1px solid #333', paddingBottom: '1rem', marginBottom: '2rem' }}>
        <h1 style={{ margin: 0, color: '#00d2ff', letterSpacing: '1px' }}>MacMS Alpha</h1>
        <p style={{ margin: '0.5rem 0 0 0', color: '#888' }}>Armoury Crate for macOS</p>
      </header>

      <section style={{ backgroundColor: '#1e1e1e', padding: '1.5rem', borderRadius: '8px', border: '1px solid #333' }}>
        <h2 style={{ marginTop: 0 }}>System Status</h2>
        <p style={{ color: '#aaa', fontSize: '0.9rem' }}>
          UI is successfully mounted. Waiting to bridge with C++ WebSocket...
        </p>
        
        <div style={{ marginTop: '2rem' }}>
          <h3>Current Profile: <span style={{ color: '#00d2ff' }}>{activeProfile}</span></h3>
          
          <div style={{ display: 'flex', gap: '10px', marginTop: '1rem' }}>
            <button 
              onClick={() => setActiveProfile('Silent')}
              style={buttonStyle(activeProfile === 'Silent')}
            >
              Silent
            </button>
            <button 
              onClick={() => setActiveProfile('Normal')}
              style={buttonStyle(activeProfile === 'Normal')}
            >
              Normal
            </button>
            <button 
              onClick={() => setActiveProfile('Performance')}
              style={buttonStyle(activeProfile === 'Performance')}
            >
              Performance
            </button>
            <button 
              onClick={() => setActiveProfile('Maximum')}
              style={buttonStyle(activeProfile === 'Maximum', true)}
            >
              Maximum
            </button>
          </div>
        </div>
      </section>
    </div>
  );
}

// Simple inline styling function to keep our Alpha clean without CSS files
function buttonStyle(isActive, isDanger = false) {
  return {
    padding: '8px 16px',
    backgroundColor: isActive ? (isDanger ? '#ff4444' : '#00d2ff') : '#2a2a2a',
    color: isActive ? '#000' : '#fff',
    border: 'none',
    borderRadius: '4px',
    cursor: 'pointer',
    fontWeight: isActive ? 'bold' : 'normal',
    transition: 'background-color 0.2s'
  };
}