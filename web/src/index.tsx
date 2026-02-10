/* @refresh reload */
import { render } from 'solid-js/web'
import { Router, Route } from '@solidjs/router'
import './index.css'
import App from './App.tsx'
import Settings from './Settings'
import Status from './Status'
import History from './History'
import Logs from './Logs'
import Update from './Update'
import About from './About'

const root = document.getElementById('root')

render(() => (
  <Router root={App}>
    <Route path="/" component={Status} />
    <Route path="/status" component={Status} />
    <Route path="/settings" component={Settings} />
    <Route path="/history" component={History} />
    <Route path="/log" component={Logs} />
    <Route path="/updates" component={Update} />
    <Route path="/about" component={About} />
  </Router>
), root!)
