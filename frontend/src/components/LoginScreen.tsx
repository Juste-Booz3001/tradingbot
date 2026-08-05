import { useState } from 'react';
import { login } from '../lib/auth';

interface LoginScreenProps {
  onSuccess: () => void;
}

export default function LoginScreen({ onSuccess }: LoginScreenProps) {
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError(null);
    setLoading(true);
    try {
      await login(username, password);
      onSuccess();
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Échec de connexion');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="flex min-h-screen items-center justify-center bg-void px-4">
      <form
        onSubmit={handleSubmit}
        className="w-full max-w-sm border border-hair bg-panel px-8 py-10"
      >
        <div className="mb-8 flex items-center gap-3">
          <span className="h-2.5 w-2.5 animate-led rounded-full bg-amber text-amber shadow-led" />
          <h1 className="font-mono text-sm font-semibold tracking-[0.2em] text-ink">
            TRADINGBOT
          </h1>
        </div>

        <label className="block font-mono text-[11px] uppercase tracking-widest text-muted">
          Identifiant
        </label>
        <input
          type="text"
          value={username}
          onChange={(e) => setUsername(e.target.value)}
          autoFocus
          className="mt-1.5 mb-4 w-full rounded-sm border border-hair bg-panel2 px-3 py-2 font-mono text-sm text-ink outline-none focus:border-amber"
        />

        <label className="block font-mono text-[11px] uppercase tracking-widest text-muted">
          Mot de passe
        </label>
        <input
          type="password"
          value={password}
          onChange={(e) => setPassword(e.target.value)}
          className="mt-1.5 mb-6 w-full rounded-sm border border-hair bg-panel2 px-3 py-2 font-mono text-sm text-ink outline-none focus:border-amber"
        />

        {error && (
          <p className="mb-4 font-mono text-xs text-sell" role="alert">
            {error}
          </p>
        )}

        <button
          type="submit"
          disabled={loading}
          className="w-full rounded-sm border border-amber/60 bg-amber/10 py-2 font-mono text-xs font-semibold uppercase tracking-widest text-amber transition-colors hover:bg-amber/20 disabled:opacity-50"
        >
          {loading ? 'Connexion…' : 'Se connecter'}
        </button>
      </form>
    </div>
  );
}
