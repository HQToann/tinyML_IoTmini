"""Tao va huan luyen model nho 2 -> 8 -> 3."""

import tensorflow as tf

from config import CLASS_NAMES, FEATURES, SEED


def train_model(data):
    tf.keras.utils.set_random_seed(SEED)
    model = tf.keras.Sequential([
        tf.keras.layers.Input(shape=(len(FEATURES),)),
        tf.keras.layers.Dense(8, activation="relu"),
        tf.keras.layers.Dense(len(CLASS_NAMES), activation="softmax"),
    ])
    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=0.01),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    model.fit(
        data["X_train"],
        data["y_train"],
        validation_data=(data["X_validation"], data["y_validation"]),
        epochs=150,
        batch_size=32,
        verbose=0,
        callbacks=[tf.keras.callbacks.EarlyStopping(
            monitor="val_loss", patience=15, restore_best_weights=True
        )],
    )
    return model
