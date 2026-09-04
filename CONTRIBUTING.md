# Como trabalhar neste repositório

## 1. Preparar o clone (uma vez só)

```bash
pip install pre-commit commitizen
pre-commit install --hook-type pre-commit --hook-type commit-msg
```

Sem isso os ganchos não rodam e mensagens fora do padrão entram no histórico —
e aí o `CHANGELOG` e o `cz bump` param de funcionar.

---

## 2. Fluxo de branches

```
main ──────────────────────────────────────●──────────────●─  tags v1.0.0, v1.1.0
   \                                      /              /
    develop ──●────────●────────●────────●──────────────●
               \      / \      / \      /
                feat/a   feat/b   fix/c
```

| Branch | Para quê |
|---|---|
| `main` | só código que já rodou na placa. Cada commit aqui vira uma tag |
| `develop` | integração — é onde as features se encontram |
| `feat/<assunto>` | uma funcionalidade nova, saindo de `develop` |
| `fix/<assunto>` | correção, saindo de `develop` |
| `hotfix/<assunto>` | correção urgente, saindo de `main` |

**Sempre feche a branch com merge `--no-ff`.** É o `--no-ff` que preserva a
"bolha" da branch no gráfico; com fast-forward os commits viram uma fila reta e
você perde a informação de o que foi feito junto.

```bash
git switch develop
git switch -c feat/nome-curto

# ... trabalha, commita ...

git switch develop
git merge --no-ff feat/nome-curto -m "feat(merge): integra feat/nome-curto na develop"
git branch -d feat/nome-curto
git push origin develop
```

A branch pode ser apagada depois do merge — o commit de merge segura o desenho
no gráfico de qualquer jeito.

---

## 3. Mensagem de commit — Conventional Commits

```
tipo(escopo): descricao no imperativo, minuscula, sem ponto final
```

| Tipo | Quando |
|---|---|
| `feat` | funcionalidade nova |
| `fix` | correção de bug |
| `docs` | só documentação |
| `test` | teste ou instrumentação de diagnóstico |
| `refactor` | reorganização sem mudar comportamento |
| `perf` | desempenho |
| `chore` | build, dependência, configuração de repo |

O **escopo** é o subsistema tocado. Neste projeto:
`motors`, `sensors`, `brain`, `face`, `sound`, `web`, `config`, `docs`, `ci`.

Exemplos reais deste repositório:

```
feat(motors): paralela os canais da DRV8833 e dobra a corrente por lado
fix(sensors): separa caminho digital e analogico do IR de borda
test(diag): painel de bancada no PC lendo a serial
docs(schematic): refaz a folha de motores com os canais em paralelo
```

Mudança que quebra compatibilidade leva `!` antes dos dois-pontos
(`feat(config)!: renomeia pinos de motor`) — isso faz o `cz bump` subir a
versão *major*.

---

## 4. Fechar uma versão

```bash
git switch main
git merge --no-ff develop -m "chore(release): fecha a versao"
cz bump --changelog          # sobe version.h, escreve CHANGELOG.md, cria a tag
git push origin main --follow-tags
```

O `cz bump` lê os tipos de commit desde a última tag e decide sozinho se a
versão sobe em *patch*, *minor* ou *major*.
