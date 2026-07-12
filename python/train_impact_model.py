"""
Fathom -- Market Impact Direction Classifier

Predicts the DIRECTION (up/down) of short-horizon (20-message) price movement
following a real execution, from pre-trade order book state.

Design notes, and why this is a classifier rather than a regressor:
A regression model targeting exact price-move magnitude was tried first and
evaluated honestly (see README's "Model Evaluation" section for the full
comparison). It showed a positive but modest R^2 (0.21 overall, 0.14 with the
top 1% most extreme moves excluded, confirming the signal wasn't purely
outlier-driven) but its MAE did not meaningfully beat a naive zero-move
baseline on typical-sized moves -- meaning it wasn't well-calibrated for
predicting exact magnitude. Directional accuracy, however, was strong (88%)
and robust. This is consistent with published market microstructure research:
direction from order-flow imbalance is a substantially more tractable signal
than precise impact magnitude. The model here is deliberately reframed to
match where the real, defensible signal actually lives.

Train/test split is strictly CHRONOLOGICAL (first 70% of the trading day for
training, last 30% for testing) -- never randomly shuffled, since a random
split on time-series data leaks future information into training.
"""
import pandas as pd
import numpy as np
import lightgbm as lgb
from sklearn.metrics import accuracy_score, roc_auc_score, classification_report
import sys

FEATURES = ["trade_qty", "direction", "spread", "bid_depth_top3", "ask_depth_top3", "imbalance"]

def load_data(path):
    df = pd.read_csv(path).sort_values("time").reset_index(drop=True)
    df = df[df["label_price_move"] != 0].reset_index(drop=True)  # zero-moves have no defined direction
    df["label_direction"] = (df["label_price_move"] > 0).astype(int)
    return df

def chronological_split(df, train_frac=0.7):
    split_idx = int(len(df) * train_frac)
    return df.iloc[:split_idx], df.iloc[split_idx:]

def train_and_evaluate(train_df, test_df):
    clf = lgb.LGBMClassifier(
        n_estimators=200, max_depth=4, learning_rate=0.05,
        min_child_samples=20, random_state=42, verbosity=-1,
    )
    clf.fit(train_df[FEATURES], train_df["label_direction"])

    preds = clf.predict(test_df[FEATURES])
    probs = clf.predict_proba(test_df[FEATURES])[:, 1]

    baseline = max(test_df["label_direction"].mean(), 1 - test_df["label_direction"].mean())

    print(f"train: {len(train_df)} rows | test: {len(test_df)} rows (chronological split)")
    print(f"\ntest accuracy:        {accuracy_score(test_df['label_direction'], preds):.3f}")
    print(f"test ROC-AUC:         {roc_auc_score(test_df['label_direction'], probs):.3f}")
    print(f"majority-class baseline: {baseline:.3f}")
    print("\n" + classification_report(test_df["label_direction"], preds, target_names=["down", "up"]))

    return clf

if __name__ == "__main__":
    path = sys.argv[1] if len(sys.argv) > 1 else "../data/AMZN_impact_features.csv"
    df = load_data(path)
    train_df, test_df = chronological_split(df)
    model = train_and_evaluate(train_df, test_df)
    model.booster_.save_model("impact_direction_classifier.txt")
    print("\nmodel saved to impact_direction_classifier.txt")
