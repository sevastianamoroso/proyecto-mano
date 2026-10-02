#ifndef KALMAN_FILTER_H
#define KALMAN_FILTER_H

/**
 * @brief Implementación del Filtro de Kalman lineal de 1 dimensión para fusión
 * de acelerómetro y giróscopo (modelo clásico de Lauszus).
 */
class KalmanFilter {
public:
    KalmanFilter() {
        Q_angle = 0.001f;
        Q_bias = 0.003f;
        R_measure = 0.03f;

        angle = 0.0f;
        bias = 0.0f;

        P[0][0] = 0.0f;
        P[0][1] = 0.0f;
        P[1][0] = 0.0f;
        P[1][1] = 0.0f;
    }

    /**
     * @brief Estima el ángulo a partir de la nueva medición y velocidad angular.
     * @param newAngle Ángulo medido por el acelerómetro (grados).
     * @param newRate Velocidad angular medida por el giróscopo (grados/segundo).
     * @param dt Paso de tiempo transcurrido en segundos.
     * @return float Ángulo estimado libre de ruido y deriva.
     */
    float getAngle(float newAngle, float newRate, float dt) {
        // 1. Etapa de Predicción
        rate = newRate - bias;
        angle += dt * rate;

        // Actualización de la matriz de covarianza de error a priori P
        P[0][0] += dt * (dt * P[1][1] - P[0][1] - P[1][0] + Q_angle);
        P[0][1] -= dt * P[1][1];
        P[1][0] -= dt * P[1][1];
        P[1][1] += Q_bias * dt;

        // 2. Etapa de Actualización (Corrección)
        float S = P[0][0] + R_measure;      // Covarianza de innovación
        float K[2];                        // Ganancia de Kalman
        K[0] = P[0][0] / S;
        K[1] = P[1][0] / S;

        float y = newAngle - angle;        // Innovación (residuo de medición)
        angle += K[0] * y;                 // Estimación a posteriori del ángulo
        bias  += K[1] * y;                 // Estimación a posteriori del sesgo (drift)

        // Actualización de la matriz de covarianza a posteriori
        float P00_temp = P[0][0];
        float P01_temp = P[0][1];

        P[0][0] -= K[0] * P00_temp;
        P[0][1] -= K[0] * P01_temp;
        P[1][0] -= K[1] * P00_temp;
        P[1][1] -= K[1] * P01_temp;

        return angle;
    }

    void setAngle(float newAngle) {
        angle = newAngle;
    }

    float getRate() const {
        return rate;
    }

    void setQangle(float newQangle) { Q_angle = newQangle; }
    void setQbias(float newQbias) { Q_bias = newQbias; }
    void setRmeasure(float newRmeasure) { R_measure = newRmeasure; }

private:
    float Q_angle;    // Varianza del proceso (acelerómetro)
    float Q_bias;     // Varianza de la deriva del giróscopo
    float R_measure;  // Varianza del ruido de medición
    float angle;      // Ángulo calculado
    float bias;       // Sesgo calculado del giróscopo
    float rate;       // Velocidad angular no sesgada
    float P[2][2];    // Matriz de covarianza del error
};

#endif // KALMAN_FILTER_H
