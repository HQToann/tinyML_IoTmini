"""Chay day du quy trinh huan luyen model goc nghieng."""

from export_model import export_and_evaluate
from load_data import load_dataset
from prepare import prepare_data
from train_model import train_model


def main():
    X, y, groups = load_dataset()
    print(f"Da doc {len(y)} dong, {X.shape[1]} dau vao.")

    data = prepare_data(X, y, groups)
    print(
        "So phien train/validation/test: "
        f"{data['train_runs']}/{data['validation_runs']}/{data['test_runs']}"
    )

    model = train_model(data)
    print(f"So tham so model: {model.count_params()}")
    export_and_evaluate(model, data)


if __name__ == "__main__":
    main()
