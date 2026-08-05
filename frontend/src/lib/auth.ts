const TOKEN_KEY = 'tradingbot_token';

export function getToken(): string | null {
  return localStorage.getItem(TOKEN_KEY);
}

export function setToken(token: string): void {
  localStorage.setItem(TOKEN_KEY, token);
}

export function clearToken(): void {
  localStorage.removeItem(TOKEN_KEY);
}

export async function login(username: string, password: string): Promise<string> {
  const res = await fetch('/api/login', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ username, password })
  });
  if (!res.ok) {
    throw new Error(res.status === 401 ? 'Identifiants invalides' : "Échec de connexion au serveur");
  }
  const data = await res.json();
  setToken(data.token);
  return data.token as string;
}

// Wrapper fetch qui attache le token courant et déclenche onUnauthorized
// (retour à l'écran de login) si le serveur répond 401 — token expiré ou révoqué.
export async function authFetch(
  input: RequestInfo,
  init: RequestInit = {},
  onUnauthorized?: () => void
): Promise<Response> {
  const token = getToken();
  const headers = new Headers(init.headers);
  if (token) headers.set('Authorization', `Bearer ${token}`);

  const res = await fetch(input, { ...init, headers });
  if (res.status === 401) {
    clearToken();
    onUnauthorized?.();
  }
  return res;
}
