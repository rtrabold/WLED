# WLED para RGBBridge

Versión personalizada usada por [RGBBridge](https://github.com/rtrabold/RGBBridge).
Proyecto original: https://github.com/wled/WLED. Se conserva su licencia MIT.

Esta instantánea contiene el firmware WLED 16.0.1 que estamos utilizando, con:

- RGBBridge EventFX 1.4.3: animaciones locales por evento y por segmento.
- RGBBridge HDD LED y AudioReactive.
- Transporte USB serial con JSON no bloqueante, diagnóstico UART y lectura
  de colores para la iluminación de RAM/GPU.
- Configuración de compilación `esp32dev` con los usermods habilitados.

## Compilar

Requiere Node.js y PlatformIO. Desde esta carpeta:

```powershell
npm ci
npm run build
pio run -e esp32dev
```

`platformio_override.ini` contiene sólo la selección de módulos; se versiona
intencionalmente. `my_config.h`, credenciales Wi-Fi, compilaciones, respaldos del
dispositivo y presets personales quedan fuera de Git.

La rama `rgbbridge` conserva nuestras modificaciones sobre la base `v16.0.1`.
`origin` es el fork público rtrabold/WLED; `upstream` identifica el proyecto
original y no debe recibir nuestros pushes. La referencia de futuras actualizaciones
es la rama principal de upstream, no una versión fijada. Para actualizar, crear una rama de
trabajo desde `rgbbridge`, traer la rama principal de `upstream`, resolver sus
cambios y probar antes de desplegar. No reemplazar directamente la rama por la
versión original, porque se perderían nuestros módulos y transporte.

Hyperion se mantiene aparte en https://github.com/rtrabold/Hyperion.
