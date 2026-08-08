import { useEffect, useRef, useState } from 'react';
import {
  createChart,
  IChartApi,
  ISeriesApi,
  CandlestickData,
  UTCTimestamp,
  MouseEventParams
} from 'lightweight-charts';
import { fmtUsd } from '../lib/format';

interface CandlestickChartProps {
  symbol: string;
  lastTick: { price: number; ts: number; fastMa?: number; slowMa?: number } | null;
}

const TIMEFRAMES: { label: string; seconds: number }[] = [
  { label: '1m', seconds: 60 },
  { label: '5m', seconds: 300 },
  { label: '15m', seconds: 900 },
  { label: '1h', seconds: 3600 }
];

// Reconstruit des chandeliers OHLC à partir du flux de ticks (le backend
// n'expose pas d'historique de chandeliers pour l'instant — voir le TODO en
// bas du fichier). Le chandelier en cours de formation se met à jour en
// direct, un nouveau démarre dès que le tick suivant tombe dans le bucket
// de temps suivant.
export default function CandlestickChart({ symbol, lastTick }: CandlestickChartProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const candleSeriesRef = useRef<ISeriesApi<'Candlestick'> | null>(null);
  const fastMaSeriesRef = useRef<ISeriesApi<'Line'> | null>(null);
  const slowMaSeriesRef = useRef<ISeriesApi<'Line'> | null>(null);
  const currentCandleRef = useRef<CandlestickData | null>(null);

  const [timeframeSec, setTimeframeSec] = useState(60);
  const [hoverData, setHoverData] = useState<CandlestickData | null>(null);

  // Chart neuf à chaque changement de symbole ou de timeframe : on repart
  // d'un historique vide plutôt que de mélanger des bougies de granularités
  // différentes dans la même série.
  useEffect(() => {
    if (!containerRef.current) return;
    currentCandleRef.current = null;
    setHoverData(null);

    const chart = createChart(containerRef.current, {
      width: containerRef.current.clientWidth,
      height: 320,
      layout: { background: { color: 'transparent' }, textColor: '#6f7887', fontFamily: 'IBM Plex Mono' },
      grid: {
        vertLines: { color: '#1a1f29' },
        horzLines: { color: '#1a1f29' }
      },
      rightPriceScale: { borderColor: '#232935' },
      timeScale: { borderColor: '#232935', timeVisible: true, secondsVisible: timeframeSec < 60 },
      crosshair: { mode: 0 } // 0 = Normal, aimante légèrement aux bougies
    });

    const candleSeries = chart.addCandlestickSeries({
      upColor: '#35c48a',
      downColor: '#e2503f',
      borderUpColor: '#35c48a',
      borderDownColor: '#e2503f',
      wickUpColor: '#35c48a',
      wickDownColor: '#e2503f'
    });

    const fastMaSeries = chart.addLineSeries({
      color: '#4fb8c9',
      lineWidth: 1,
      priceLineVisible: false,
      lastValueVisible: false,
      crosshairMarkerVisible: false
    });
    const slowMaSeries = chart.addLineSeries({
      color: '#e2a63b',
      lineWidth: 1,
      priceLineVisible: false,
      lastValueVisible: false,
      crosshairMarkerVisible: false
    });

    chart.subscribeCrosshairMove((param: MouseEventParams) => {
      const candle = param.seriesData.get(candleSeries) as CandlestickData | undefined;
      setHoverData(candle ?? null);
    });

    chartRef.current = chart;
    candleSeriesRef.current = candleSeries;
    fastMaSeriesRef.current = fastMaSeries;
    slowMaSeriesRef.current = slowMaSeries;

    const handleResize = () => {
      if (containerRef.current) chart.applyOptions({ width: containerRef.current.clientWidth });
    };
    window.addEventListener('resize', handleResize);

    return () => {
      window.removeEventListener('resize', handleResize);
      chart.remove();
    };
  }, [symbol, timeframeSec]);

  useEffect(() => {
    if (!lastTick || !candleSeriesRef.current) return;

    const bucketTime = (Math.floor(lastTick.ts / 1000 / timeframeSec) * timeframeSec) as UTCTimestamp;
    const current = currentCandleRef.current;

    const candle: CandlestickData =
      current && current.time === bucketTime
        ? {
            time: bucketTime,
            open: current.open,
            high: Math.max(current.high, lastTick.price),
            low: Math.min(current.low, lastTick.price),
            close: lastTick.price
          }
        : {
            time: bucketTime,
            open: current?.close ?? lastTick.price, // enchaîne sur la clôture précédente si connue
            high: lastTick.price,
            low: lastTick.price,
            close: lastTick.price
          };

    currentCandleRef.current = candle;
    candleSeriesRef.current.update(candle);

    const t = Math.floor(lastTick.ts / 1000) as UTCTimestamp;
    if (lastTick.fastMa !== undefined) fastMaSeriesRef.current?.update({ time: t, value: lastTick.fastMa });
    if (lastTick.slowMa !== undefined) slowMaSeriesRef.current?.update({ time: t, value: lastTick.slowMa });
  }, [lastTick, timeframeSec]);

  const displayed = hoverData ?? currentCandleRef.current;
  const isUp = displayed ? displayed.close >= displayed.open : true;

  return (
    <div className="border-t border-hair bg-panel px-6 py-5">
      <div className="flex flex-wrap items-center justify-between gap-3">
        <div>
          <p className="font-mono text-[11px] uppercase tracking-widest text-muted">
            {symbol} — chandeliers
          </p>
          {displayed && (
            <p className={`mt-1 font-mono text-xs ${isUp ? 'text-buy' : 'text-sell'}`}>
              O {fmtUsd(displayed.open)} · H {fmtUsd(displayed.high)} · L {fmtUsd(displayed.low)} · C{' '}
              {fmtUsd(displayed.close)}
            </p>
          )}
        </div>
        <div className="flex overflow-hidden rounded-sm border border-hair font-mono text-xs">
          {TIMEFRAMES.map((tf) => (
            <button
              key={tf.seconds}
              onClick={() => setTimeframeSec(tf.seconds)}
              className={`px-2.5 py-1 transition-colors ${
                timeframeSec === tf.seconds ? 'bg-amber/20 text-amber' : 'bg-panel2 text-muted hover:text-ink'
              }`}
            >
              {tf.label}
            </button>
          ))}
        </div>
      </div>
      <p className="mt-1 font-mono text-[10px] text-muted/70">
        Reconstruit à partir du flux de ticks depuis l'ouverture du dashboard — pas d'historique de
        chandeliers persistant pour l'instant.
      </p>
      <div ref={containerRef} className="mt-3" />
    </div>
  );
}

// TODO backend : exposer un /api/candles?symbol=...&interval=... basé sur
// market_data (déjà en base via Database::insertTick) pour que ce graphique
// affiche un historique réel à l'ouverture, au lieu de repartir de zéro à
// chaque rechargement de page.
