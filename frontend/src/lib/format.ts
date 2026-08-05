export function fmtUsd(n: number, decimals = 2): string {
  return n.toLocaleString('fr-FR', {
    minimumFractionDigits: decimals,
    maximumFractionDigits: decimals
  });
}

export function fmtPct(n: number, decimals = 2): string {
  const sign = n > 0 ? '+' : '';
  return `${sign}${n.toFixed(decimals)}%`;
}

export function fmtQty(n: number): string {
  return n.toLocaleString('fr-FR', { maximumFractionDigits: 6 });
}

export function fmtTime(ts: number | string): string {
  const d = typeof ts === 'number' ? new Date(ts) : new Date(ts);
  return d.toLocaleTimeString('fr-FR', { hour12: false });
}

export function fmtDateTime(ts: number | string): string {
  const d = typeof ts === 'number' ? new Date(ts) : new Date(ts);
  return d.toLocaleString('fr-FR', { hour12: false });
}
