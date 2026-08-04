"""
Dashboard Streamlit pour TradingBot.

Interroge l'API REST du moteur C++ (http://localhost:8080 par défaut) toutes
les 2 secondes. C'est du polling, pas un vrai push WebSocket — largement
suffisant pour de la supervision, pas pour du scalping visuel à la milliseconde.

Lancement :
    streamlit run dashboard/streamlit_app.py
"""

import requests
import pandas as pd
import plotly.graph_objects as go
import streamlit as st
from streamlit_autorefresh import st_autorefresh

API_BASE = st.sidebar.text_input("URL de l'API backend", "http://localhost:8080")
REFRESH_MS = st.sidebar.slider("Rafraîchissement (secondes)", 1, 10, 2) * 1000

st.set_page_config(page_title="TradingBot Dashboard", layout="wide")
st_autorefresh(interval=REFRESH_MS, key="autorefresh")

st.title("📊 TradingBot — Supervision")


def safe_get(endpoint: str, default=None):
    try:
        resp = requests.get(f"{API_BASE}{endpoint}", timeout=3)
        resp.raise_for_status()
        return resp.json()
    except requests.RequestException as e:
        st.session_state["last_error"] = str(e)
        return default


def safe_post(endpoint: str) -> bool:
    try:
        resp = requests.post(f"{API_BASE}{endpoint}", timeout=3)
        resp.raise_for_status()
        return True
    except requests.RequestException as e:
        st.error(f"Échec de l'appel {endpoint} : {e}")
        return False


# --- Statut global ---
status = safe_get("/api/status")

if status is None:
    st.error(
        "Impossible de joindre le backend C++. Vérifiez qu'il tourne "
        f"(`./tradingbot_engine config.json`) et que l'URL ci-contre est correcte."
    )
else:
    col1, col2, col3, col4 = st.columns(4)
    col1.metric("Équité", f"{status['equity']:.2f} $")
    col2.metric("Drawdown", f"{status['drawdown_pct']:.2f} %")
    col3.metric("État", "🔴 Arrêté" if status["halted"] else "🟢 Actif")

    with col4:
        st.write("")  # alignement vertical avec les metrics
        if status["halted"]:
            if st.button("▶️ Reprendre le trading", use_container_width=True):
                if safe_post("/api/resume"):
                    st.rerun()
        else:
            if st.button("🛑 Arrêt d'urgence", type="primary", use_container_width=True):
                if safe_post("/api/halt"):
                    st.rerun()

st.divider()

# --- Courbe d'équité ---
st.subheader("Courbe d'équité")
equity_data = safe_get("/api/equity_history", default={"points": []})
points = equity_data.get("points", [])

if points:
    df = pd.DataFrame(points)
    df["ts"] = pd.to_datetime(df["ts"])

    fig = go.Figure()
    fig.add_trace(go.Scatter(
        x=df["ts"], y=df["equity"], mode="lines", name="Équité",
        line=dict(width=2)
    ))
    fig.update_layout(
        height=350,
        margin=dict(l=10, r=10, t=10, b=10),
        yaxis_title="Équité ($)",
        showlegend=False,
    )
    st.plotly_chart(fig, use_container_width=True)

    fig_dd = go.Figure()
    fig_dd.add_trace(go.Scatter(
        x=df["ts"], y=-df["drawdown_pct"], mode="lines", name="Drawdown",
        fill="tozeroy", line=dict(width=1, color="crimson")
    ))
    fig_dd.update_layout(
        height=180,
        margin=dict(l=10, r=10, t=10, b=10),
        yaxis_title="Drawdown (%)",
        showlegend=False,
    )
    st.plotly_chart(fig_dd, use_container_width=True)
else:
    st.info("Aucune donnée d'équité pour l'instant — le bot doit tourner au moins quelques cycles.")

st.divider()

# --- Trades récents ---
st.subheader("Trades récents")
trades_data = safe_get("/api/trades", default={"trades": []})
trades = trades_data.get("trades", [])

if trades:
    tdf = pd.DataFrame(trades)
    tdf["ts"] = pd.to_datetime(tdf["ts"])
    tdf = tdf.rename(columns={
        "symbol": "Symbole", "side": "Sens", "quantity": "Quantité",
        "fill_price": "Prix", "pnl": "PnL", "ts": "Horodatage"
    })

    def highlight_pnl(val):
        color = "#16a34a" if val > 0 else ("#dc2626" if val < 0 else "")
        return f"color: {color}"

    st.dataframe(
        tdf.style.map(highlight_pnl, subset=["PnL"]),
        use_container_width=True,
        hide_index=True,
    )
else:
    st.info("Aucun trade pour l'instant.")
