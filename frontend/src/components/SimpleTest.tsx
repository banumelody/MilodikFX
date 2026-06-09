export function SimpleTest() {
  return (
    <div style={{ 
      width: '100vw', 
      height: '100vh', 
      backgroundColor: '#000', 
      color: '#fff',
      display: 'flex',
      flexDirection: 'column',
      alignItems: 'center',
      justifyContent: 'center',
      fontFamily: 'Arial, sans-serif'
    }}>
      <h1>✅ React is Working!</h1>
      <p style={{ fontSize: '20px', marginTop: '20px' }}>Component loaded successfully</p>
      <p style={{ fontSize: '16px', color: '#888', marginTop: '40px' }}>
        If you see this, React rendering works.
      </p>
      <button 
        onClick={() => alert('Button clicked!')}
        style={{
          marginTop: '30px',
          padding: '10px 20px',
          fontSize: '16px',
          backgroundColor: '#00bfff',
          color: '#000',
          border: 'none',
          borderRadius: '4px',
          cursor: 'pointer'
        }}
      >
        Click Me
      </button>
    </div>
  );
}
