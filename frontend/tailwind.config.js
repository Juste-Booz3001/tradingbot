/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{js,ts,jsx,tsx}'],
  theme: {
    extend: {
      colors: {
        void: '#0a0c10',
        panel: '#12161d',
        panel2: '#161b24',
        hair: '#232935',
        hair2: '#2d3543',
        ink: '#e7ebf1',
        muted: '#6f7887',
        amber: {
          DEFAULT: '#e2a63b',
          dim: '#8a692e',
          glow: '#f3c469'
        },
        buy: '#35c48a',
        sell: '#e2503f'
      },
      fontFamily: {
        sans: ['"IBM Plex Sans"', 'system-ui', 'sans-serif'],
        mono: ['"IBM Plex Mono"', '"JetBrains Mono"', 'monospace']
      },
      boxShadow: {
        led: '0 0 8px currentColor'
      }
    }
  },
  plugins: []
};
