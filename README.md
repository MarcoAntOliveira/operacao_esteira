## Operação da esteira

Este repositorio visa impementar a operação a esteira usando uma maquina de estdos 


```mermaid
    stateDiagram-v2
    [*] --> detecta_peca

    detecta_peca --> inicia_esteira:sim(sensor no inicio da esteira)
    inicia_esteira --> sensor_meio : sim
    sensor_meio --> sensor_fim : sim
    sensor_fim --> aciona_ur:sim
    aciona_ur --> detecta_peca
    
```