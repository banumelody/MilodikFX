import { useTheme } from './hooks';
import { DashboardV2 } from './components';

function App() {
  const { theme, toggleTheme } = useTheme();

  return (
    <div className="w-full h-screen bg-gray-950 text-white">
      {/* Dashboard */}
      <DashboardV2 />

      {/* Theme Toggle - Always accessible */}
      <button
        onClick={toggleTheme}
        className="fixed top-4 right-4 p-2 bg-gray-800 hover:bg-gray-700 rounded-lg z-40 transition-colors"
        title={`Switch to ${theme === 'dark' ? 'light' : 'dark'} mode`}
      >
        {theme === 'dark' ? '☀️' : '🌙'}
      </button>
    </div>
  );
}

export default App;
